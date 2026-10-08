// Taiwanese Enterprises LLC — interactions
(function () {
  "use strict";

  var BUSINESS_EMAIL = "taiwanenterprisellc@gmail.com";

  // ---- Mobile nav toggle ----
  var toggle = document.getElementById("navToggle");
  var links = document.getElementById("navLinks");
  if (toggle && links) {
    toggle.addEventListener("click", function () {
      var open = links.classList.toggle("open");
      toggle.classList.toggle("active", open);
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    links.querySelectorAll("a").forEach(function (a) {
      a.addEventListener("click", function () {
        links.classList.remove("open");
        toggle.classList.remove("active");
        toggle.setAttribute("aria-expanded", "false");
      });
    });
  }

  // ---- Reveal on scroll ----
  var revealEls = document.querySelectorAll(".card, .g-item, .about-copy, .about-img, .contact-copy, .quote-form");
  revealEls.forEach(function (el) { el.classList.add("reveal"); });

  if ("IntersectionObserver" in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) {
          entry.target.classList.add("visible");
          io.unobserve(entry.target);
        }
      });
    }, { threshold: 0.12 });
    revealEls.forEach(function (el) { io.observe(el); });
  } else {
    revealEls.forEach(function (el) { el.classList.add("visible"); });
  }

  // ---- Quote form: backend API first, email fallback ----
  // Tries POST /api/quote (the C++ server). When the site is on plain
  // static hosting with no backend, the fetch fails and we fall back
  // to the original mailto behavior so the form never breaks.
  var form = document.getElementById("quoteForm");
  var err = document.getElementById("formError");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();
      var name = document.getElementById("qName").value.trim();
      var phone = document.getElementById("qPhone").value.trim();
      var email = document.getElementById("qEmail").value.trim();
      var service = document.getElementById("qService").value;
      var message = document.getElementById("qMsg").value.trim();

      var emailOk = /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
      if (!name || !phone || !emailOk || !service) {
        err.hidden = false;
        return;
      }
      err.hidden = true;

      function mailtoFallback() {
        var subject = "Quote Request — " + service + " — " + name;
        var bodyLines = [
          "New quote request from the Taiwanese Enterprises LLC website.",
          "",
          "Name: " + name,
          "Phone: " + phone,
          "Email: " + email,
          "Service: " + service,
          "",
          "Job details:",
          message || "(none provided)"
        ];
        var href = "mailto:" + BUSINESS_EMAIL
          + "?subject=" + encodeURIComponent(subject)
          + "&body=" + encodeURIComponent(bodyLines.join("\n"));
        window.location.href = href;
      }

      function showSent() {
        form.innerHTML = '<p class="form-success" role="status">Request received. We will call you back soon.</p>';
      }

      if (!("fetch" in window)) {
        mailtoFallback();
        return;
      }

      fetch("/api/quote", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name: name,
          phone: phone,
          email: email,
          service: service,
          details: message
        })
      }).then(function (resp) {
        if (!resp.ok) throw new Error("bad status " + resp.status);
        return resp.json();
      }).then(function (result) {
        if (result && result.ok) {
          showSent();
        } else {
          mailtoFallback();
        }
      }).catch(function () {
        mailtoFallback();
      });
    });
  }
})();
