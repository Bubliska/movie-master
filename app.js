'use strict';

const donationDialog = document.querySelector('#donation-dialog');
document.querySelector('#donate-button').addEventListener('click', () => {
  donationDialog.showModal();
});
document.querySelectorAll('[data-copy-address]').forEach(button => {
  button.addEventListener('click', async () => {
    const address = document.getElementById(button.dataset.copyAddress).textContent.trim();
    const status = button.nextElementSibling;
    try {
      await navigator.clipboard.writeText(address);
      status.textContent = 'Address copied.';
    } catch {
      status.textContent = 'Please select and copy the address above.';
    }
  });
});
document.querySelectorAll('.dialog-close, .dialog-dismiss').forEach(button => {
  button.addEventListener('click', () => button.closest('dialog').close());
});

document.querySelector('#requirements-button').addEventListener('click', () => {
  document.querySelector('#requirements-dialog').showModal();
});
