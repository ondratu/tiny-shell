// The menu of the site: drop-downs by click (touch and keyboard), and a folded
// menu (a button and a tree) when the menu does not fit in one line.
(function () {
  var top = document.querySelector(".top");
  var bar = top.querySelector(".bar");
  var burger = top.querySelector(".burger");
  var groups = top.querySelectorAll("li.group");

  function closeGroups(except) {
    groups.forEach(function (g) {
      if (g !== except) {
        g.classList.remove("open");
        g.firstElementChild.setAttribute("aria-expanded", "false");
      }
    });
  }

  groups.forEach(function (g) {
    g.firstElementChild.addEventListener("click", function () {
      var open = !g.classList.contains("open");
      closeGroups(g);
      g.classList.toggle("open", open);
      this.setAttribute("aria-expanded", open ? "true" : "false");
    });
  });

  document.addEventListener("click", function (e) {
    if (!e.target.closest("li.group")) closeGroups(null);
  });
  document.addEventListener("keydown", function (e) {
    if (e.key === "Escape") {
      closeGroups(null);
      top.classList.remove("open");
      burger.setAttribute("aria-expanded", "false");
    }
  });

  burger.addEventListener("click", function () {
    var open = top.classList.toggle("open");
    burger.setAttribute("aria-expanded", open ? "true" : "false");
  });

  var width = 0;

  function fit() {
    // a resize of the height only (the keyboard of a phone) leaves the menu as it is
    if (window.innerWidth === width) return;
    width = window.innerWidth;
    top.classList.remove("collapsed", "open");
    burger.setAttribute("aria-expanded", "false");
    if (bar.scrollWidth > bar.clientWidth) top.classList.add("collapsed");
  }

  fit();
  window.addEventListener("resize", fit);
  if (document.fonts && document.fonts.ready) document.fonts.ready.then(fit);
})();
