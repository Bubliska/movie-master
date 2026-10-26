'use strict';

const feedbackForm = document.querySelector('#feedback-form');
const feedbackStatus = document.querySelector('#feedback-status');
const feedbackList = document.querySelector('#feedback-list');
const creatorDialog = document.querySelector('#creator-dialog');
const creatorStatus = document.querySelector('#creator-status');
const creatorList = document.querySelector('#creator-list');
let creatorPassword = '';

async function feedbackRequest(options = {}, creator = false) {
  const result = await fetch(`/api/feedback${creator ? '?admin=1' : ''}`, {
    ...options,
    headers: { 'Content-Type': 'application/json', ...(creator ? { Authorization: `Bearer ${creatorPassword}` } : {}) }
  });
  const body = await result.json();
  if (!result.ok) throw new Error(body.error || 'Please try again shortly.');
  return body;
}

function element(tag, className, content) {
  const node = document.createElement(tag);
  node.className = className;
  if (content !== undefined) node.textContent = content;
  return node;
}

function commentCard(comment) {
  const card = element('li', 'feedback-comment');
  const heading = element('div', 'comment-heading');
  heading.append(element('strong', '', comment.name));
  const date = element('time', '', new Date(comment.createdAt).toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' }));
  date.dateTime = new Date(comment.createdAt).toISOString();
  heading.append(date);
  card.append(heading, element('p', 'comment-message', comment.message));
  if (comment.reply) {
    const reply = element('div', 'comment-reply');
    reply.append(element('strong', '', 'Movie Master · Creator'), element('p', 'comment-message', comment.reply.message));
    card.append(reply);
  }
  return card;
}

async function loadFeedback() {
  try {
    const { comments } = await feedbackRequest();
    feedbackList.replaceChildren(...comments.map(commentCard));
    feedbackStatus.textContent = comments.length ? 'Showing the latest messages. Replies from the creator appear below each message.' : 'Be the first to leave a note.';
    feedbackForm.querySelector('button[type="submit"]').disabled = false;
  } catch (error) { feedbackStatus.textContent = error.message; }
}

feedbackForm.addEventListener('submit', async event => {
  event.preventDefault();
  const button = feedbackForm.querySelector('button[type="submit"]');
  button.disabled = true;
  try {
    const fields = Object.fromEntries(new FormData(feedbackForm));
    const result = await feedbackRequest({ method: 'POST', body: JSON.stringify(fields) });
    feedbackForm.reset();
    feedbackStatus.textContent = result.message;
  } catch (error) { feedbackStatus.textContent = error.message; }
  finally { button.disabled = false; }
});

async function manageComment(id, action, reply) {
  await feedbackRequest({ method: 'PATCH', body: JSON.stringify({ id, action, reply }) }, true);
  await loadCreatorMessages();
  await loadFeedback();
}

async function loadCreatorMessages() {
  const password = creatorPassword;
  if (!password || !creatorDialog.open) return;
  const { comments } = await feedbackRequest({}, true);
  if (!creatorDialog.open || password !== creatorPassword) return;
  creatorList.replaceChildren();
  for (const comment of comments) {
    const card = commentCard(comment);
    card.append(element('p', 'dialog-small', `${comment.email} · ${comment.approved ? 'Published' : 'Awaiting approval'}`));
    const form = element('form', 'creator-reply');
    const label = element('label', '', 'Your reply');
    const reply = element('textarea', '');
    reply.name = 'reply'; reply.required = true; reply.maxLength = 3000; reply.rows = 3;
    reply.value = comment.reply?.message || '';
    label.append(reply);
    const actions = element('div', 'comment-actions');
    const send = element('button', 'button primary', comment.reply ? 'Update reply' : 'Reply & publish');
    send.type = 'submit';
    actions.append(send);
    if (!comment.approved) {
      const approve = element('button', 'footer-link', 'Publish');
      approve.type = 'button';
      approve.addEventListener('click', async () => {
        approve.disabled = true;
        try { await manageComment(comment.id, 'approve'); creatorStatus.textContent = 'Message published.'; }
        catch (error) { creatorStatus.textContent = error.message; approve.disabled = false; }
      });
      actions.append(approve);
    }
    const remove = element('button', 'footer-link', 'Delete');
    remove.type = 'button';
    remove.addEventListener('click', async () => {
      if (!confirm('Delete this message and its reply permanently?')) return;
      remove.disabled = true;
      try { await manageComment(comment.id, 'delete'); creatorStatus.textContent = 'Message deleted.'; }
      catch (error) { creatorStatus.textContent = error.message; remove.disabled = false; }
    });
    actions.append(remove);
    form.append(label, actions);
    form.addEventListener('submit', async event => {
      event.preventDefault(); send.disabled = true;
      try { await manageComment(comment.id, 'reply', reply.value); creatorStatus.textContent = 'Reply published.'; }
      catch (error) { creatorStatus.textContent = error.message; send.disabled = false; }
    });
    card.append(form);
    creatorList.append(card);
  }
  document.querySelector('#creator-login').hidden = true;
  creatorStatus.textContent = comments.length ? 'Latest 50 pending and 50 published messages.' : 'No messages yet.';
}

document.querySelector('#creator-button').addEventListener('click', () => creatorDialog.showModal());
document.querySelector('#creator-login').addEventListener('submit', async event => {
  event.preventDefault();
  const button = event.target.querySelector('button'); button.disabled = true;
  creatorPassword = document.querySelector('#creator-password').value;
  try { await loadCreatorMessages(); }
  catch (error) { creatorPassword = ''; creatorStatus.textContent = error.message; }
  finally { button.disabled = false; }
});
creatorDialog.addEventListener('close', () => {
  creatorPassword = '';
  creatorList.replaceChildren();
  creatorStatus.textContent = '';
  document.querySelector('#creator-login').reset();
  document.querySelector('#creator-login').hidden = false;
});
loadFeedback();
