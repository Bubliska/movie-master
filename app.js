'use strict';

// Add the chosen payment service URL here when donations open.
const donationUrl = '';
const donationDialog = document.querySelector('#donation-dialog');
document.querySelector('#donate-button').addEventListener('click', () => {
  if (donationUrl) window.location.assign(donationUrl);
  else donationDialog.showModal();
});
document.querySelectorAll('.dialog-close, .dialog-dismiss').forEach(button => {
  button.addEventListener('click', () => button.closest('dialog').close());
});

document.querySelector('#requirements-button').addEventListener('click', () => {
  document.querySelector('#requirements-dialog').showModal();
});
