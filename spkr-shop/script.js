(() => {
  const links = document.querySelectorAll('a[href^="#"]');
  for (const link of links) {
    link.addEventListener('click', () => {
      document.body.classList.add('has-navigated');
    });
  }
})();
