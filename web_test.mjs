import { test } from 'node:test';
import assert from 'node:assert/strict';
import feedback from './api/feedback.mjs';
import download from './api/download.mjs';
import { redisFixture } from './redis_fixture.mjs';

test('feedback: private email, moderation, replies, authorization, rate limit and deletion', async () => {
  const originalFetch = globalThis.fetch;
  const originalEnv = { ...process.env };
  process.env.FEEDBACK_KV_REST_API_URL = 'https://redis.fixture';
  process.env.FEEDBACK_KV_REST_API_TOKEN = 'test-storage-token';
  process.env.FEEDBACK_ADMIN_PASSWORD = 'test-creator-password';
  const store = redisFixture();
  globalThis.fetch = async (url, options) => {
    assert.match(String(url), /^https:\/\/redis\.fixture\/(pipeline|multi-exec)$/);
    assert.equal(options.headers.Authorization, 'Bearer test-storage-token');
    return Response.json(store(JSON.parse(options.body)));
  };
  const call = (method = 'GET', body, admin = false, origin = 'https://moviemaster.test') => feedback.fetch(new Request(`https://moviemaster.test/api/feedback${admin ? '?admin=1' : ''}`, {
    method,
    headers: { 'Content-Type': 'application/json', origin, 'x-vercel-forwarded-for': '127.0.0.1', ...(admin ? { Authorization: 'Bearer test-creator-password' } : {}) },
    ...(body === undefined ? {} : { body: JSON.stringify(body) })
  }));
  try {
    const message = { name: 'Çağrı', email: 'private@example.com', message: '<script>alert(1)</script> Can you add this feature?' };
    assert.equal((await call('POST', message, false, 'https://elsewhere.test')).status, 403);
    assert.equal((await call('POST', { ...message, email: 'wrong' })).status, 400);
    assert.equal((await call('POST', { ...message, website: 'spam.test' })).status, 202);
    assert.deepEqual((await (await call()).json()).comments, []);
    assert.equal((await call('POST', message)).status, 202);
    assert.equal((await call('POST', message)).status, 429);
    assert.deepEqual((await (await call()).json()).comments, []);
    assert.equal((await feedback.fetch(new Request('https://moviemaster.test/api/feedback?admin=1'))).status, 401);
    const pending = (await (await call('GET', undefined, true)).json()).comments;
    assert.equal(pending.length, 1); assert.equal(pending[0].email, message.email); assert.equal(pending[0].approved, false);
    const id = pending[0].id;
    assert.equal((await call('PATCH', { id, action: 'approve' })).status, 401);
    assert.equal((await call('PATCH', { id, action: 'approve' }, true)).status, 200);
    let publicData = await (await call()).json();
    assert.equal(publicData.comments.length, 1); assert.ok(!JSON.stringify(publicData).includes(message.email));
    assert.equal(publicData.comments[0].message, message.message);
    assert.equal((await call('PATCH', { id, action: 'reply', reply: 'Thanks! I’m looking into it.' }, true)).status, 200);
    publicData = await (await call()).json();
    assert.equal(publicData.comments[0].reply.message, 'Thanks! I’m looking into it.');
    assert.equal((await call('PATCH', { id, action: 'delete' }, true)).status, 200);
    assert.deepEqual((await (await call()).json()).comments, []);
    assert.equal((await call('PATCH', { id, action: 'approve' }, true)).status, 404);
    globalThis.fetch = async () => { throw new Error('Storage down'); };
    assert.equal((await call()).status, 503);
    delete process.env.FEEDBACK_KV_REST_API_TOKEN;
    assert.equal((await call()).status, 503);
  } finally {
    globalThis.fetch = originalFetch;
    for (const key of Object.keys(process.env)) if (!(key in originalEnv)) delete process.env[key];
    Object.assign(process.env, originalEnv);
  }
});

test('downloads select the latest platform asset and safely fall back to releases', async () => {
  const originalFetch = globalThis.fetch;
  const base = 'https://github.com/Bubliska/movie-master/releases/download/v9.9.9/';
  try {
    globalThis.fetch = async () => Response.json({ assets: ['Movie-Master-9.9.9-AppleSilicon.dmg', 'Movie-Master-9.9.9-DOS-Windows.zip', 'SHA256.txt'].map(name => ({ name, browser_download_url: base + name })) });
    for (const [platform, file] of [['mac', 'Movie-Master-9.9.9-AppleSilicon.dmg'], ['windows', 'Movie-Master-9.9.9-DOS-Windows.zip'], ['checksums', 'SHA256.txt']]) {
      const result = await download.fetch(new Request(`https://moviemaster.test/api/download?platform=${platform}`));
      assert.equal(result.status, 307); assert.equal(result.headers.get('location'), base + file);
    }
    assert.equal((await download.fetch(new Request('https://moviemaster.test/api/download?platform=nope'))).status, 400);
    globalThis.fetch = async () => { throw new Error('API unavailable'); };
    assert.equal((await download.fetch(new Request('https://moviemaster.test/api/download?platform=mac'))).headers.get('location'), 'https://github.com/Bubliska/movie-master/releases/latest');
  } finally { globalThis.fetch = originalFetch; }
});
