import { createHash, timingSafeEqual, randomUUID } from 'node:crypto';

const prefix = 'moviemaster:feedback:';
const headers = { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' };
const response = (status, body) => new Response(JSON.stringify(body), { status, headers });
const text = value => typeof value === 'string' ? value.trim() : '';
const digest = value => createHash('sha256').update(value).digest();

async function redis(commands, transaction = false) {
  const result = await fetch(`${process.env.FEEDBACK_KV_REST_API_URL}/${transaction ? 'multi-exec' : 'pipeline'}`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${process.env.FEEDBACK_KV_REST_API_TOKEN}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(commands),
    signal: AbortSignal.timeout(8000)
  });
  const values = await result.json();
  if (!result.ok || !Array.isArray(values) || values.some(value => value.error)) throw new Error('Storage unavailable');
  return values.map(value => value.result);
}

function publicComment(comment) {
  const { email, ...visible } = comment;
  return visible;
}

export default {
  async fetch(request) {
    if (!['GET', 'POST', 'PATCH'].includes(request.method)) return response(405, { error: 'Method not allowed.' });
    if (!process.env.FEEDBACK_KV_REST_API_URL || !process.env.FEEDBACK_KV_REST_API_TOKEN || !process.env.FEEDBACK_ADMIN_PASSWORD) {
      return response(503, { error: 'Feedback is being set up. Please check back soon.' });
    }
    const url = new URL(request.url);
    const supplied = request.headers.get('authorization') || '';
    const admin = timingSafeEqual(digest(supplied), digest(`Bearer ${process.env.FEEDBACK_ADMIN_PASSWORD}`));
    if ((url.searchParams.has('admin') || request.method === 'PATCH') && !admin) return response(401, { error: 'The creator password is incorrect.' });
    if (request.method !== 'GET' && request.headers.get('origin') !== url.origin) return response(403, { error: 'Please submit feedback from this website.' });
    try {
      if (request.method === 'GET') {
        const adminView = url.searchParams.has('admin');
        const groups = adminView ? ['pending', 'public'] : ['public'];
        const ids = (await redis(groups.map(group => ['ZRANGE', prefix + group, 0, 49, 'REV']))).flat();
        const unique = [...new Set(ids)];
        const records = unique.length ? (await redis([['HMGET', prefix + 'comments', ...unique]]))[0] : [];
        const comments = records.filter(Boolean).map(JSON.parse).sort((a, b) => b.createdAt - a.createdAt);
        return response(200, { comments: adminView ? comments : comments.filter(comment => comment.approved).map(publicComment) });
      }
      if (!request.headers.get('content-type')?.startsWith('application/json')) return response(415, { error: 'Send feedback as JSON.' });
      const raw = await request.text();
      if (new TextEncoder().encode(raw).length > 16000) return response(413, { error: 'Your message is too long.' });
      let body;
      try { body = JSON.parse(raw); } catch { return response(400, { error: 'Please check your message and try again.' }); }
      if (!body || typeof body !== 'object' || Array.isArray(body)) return response(400, { error: 'Please check your message and try again.' });
      if (request.method === 'POST') {
        if (text(body.website)) return response(202, { message: 'Thank you. Your message will appear after review.' });
        const name = text(body.name), email = text(body.email), message = text(body.message);
        if (!name || name.length > 60 || /[\u0000-\u001f\u007f]/.test(name)) return response(400, { error: 'Enter a name of up to 60 characters.' });
        if (email.length > 254 || !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) return response(400, { error: 'Enter a valid email address. It will stay private.' });
        if (message.length < 5 || message.length > 3000 || /[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/.test(message)) return response(400, { error: 'Write a message between 5 and 3,000 characters.' });
        const ip = request.headers.get('x-vercel-forwarded-for') || request.headers.get('x-forwarded-for') || 'unknown';
        const rateKey = prefix + 'rate:' + digest(process.env.FEEDBACK_ADMIN_PASSWORD + ip).toString('hex');
        const [allowed] = await redis([['SET', rateKey, '1', 'NX', 'EX', 60]]);
        if (!allowed) return response(429, { error: 'Please wait a minute before leaving another message.' });
        const comment = { id: randomUUID(), name, email, message, createdAt: Date.now(), approved: false, reply: null };
        await redis([['HSET', prefix + 'comments', comment.id, JSON.stringify(comment)], ['ZADD', prefix + 'pending', comment.createdAt, comment.id]], true);
        return response(202, { message: 'Thank you. Your message will appear after review.' });
      }
      const id = text(body.id);
      if (!/^[0-9a-f-]{36}$/.test(id)) return response(400, { error: 'Choose a message to manage.' });
      const [stored] = await redis([['HGET', prefix + 'comments', id]]);
      if (!stored) return response(404, { error: 'This message no longer exists.' });
      const comment = JSON.parse(stored);
      if (body.action === 'delete') {
        await redis([['HDEL', prefix + 'comments', id], ['ZREM', prefix + 'pending', id], ['ZREM', prefix + 'public', id]], true);
      } else {
        if (body.action === 'reply') {
          const message = text(body.reply);
          if (!message || message.length > 3000) return response(400, { error: 'Write a reply of up to 3,000 characters.' });
          comment.reply = { message, createdAt: Date.now() };
        } else if (body.action !== 'approve') return response(400, { error: 'Choose approve, reply or delete.' });
        comment.approved = true;
        await redis([['HSET', prefix + 'comments', id, JSON.stringify(comment)], ['ZREM', prefix + 'pending', id], ['ZADD', prefix + 'public', comment.createdAt, id]], true);
      }
      return response(200, { ok: true });
    } catch {
      return response(503, { error: 'Feedback is temporarily unavailable. Please try again shortly.' });
    }
  }
};
