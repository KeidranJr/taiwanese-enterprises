# Quote privacy fix - prepared 2026-10-08, NOT deployed

## The problem

GET /api/quotes on the live EC2 site (http://3.145.38.246) returns every
stored quote to anyone who asks. No login, no secret. Each row holds the
customer's name, phone, email, service, and details (quote_store.cpp
INSERT/SELECT). The old code comment said it plainly: open for now, add a
check before real customer data goes through it.

Real customer data is about to go through it: a first real booking
(pressure washing, Oct 24) is on the books, and Keidran was just asked to
send a test quote through the form, which would put his own name, phone,
and email in that same public list.

## The fix (done locally, compiles, not committed, not deployed)

backend/src/main.cpp, GET /api/quotes only:
- Reads an ADMIN_TOKEN environment variable.
- The request must send that exact value in the X-Admin-Token header.
- If ADMIN_TOKEN is not set on the server, the route answers 404 - closed
  by default, so even a rushed deploy stays safe.
- POST /api/quote (the public form) is untouched. Email alerts untouched.

backend/te-backend.service:
- Added a commented Environment=ADMIN_TOKEN line with fill-in instructions,
  matching the existing Gmail settings pattern. Uncommented with a real
  secret it also gives Keidran his owner view back:
    curl -H "X-Admin-Token: the-secret" http://3.145.38.246/api/quotes

Verified: git diff is 2 files, main.cpp +19/-5. g++ syntax check passes
with the project's own crow/asio headers. Full build cannot run in this
VM (no libcurl dev files; the CMake configure error predates this change
and the real build runs on EC2).

## To ship it (needs Keidran's go-ahead)

1. Pick the ADMIN_TOKEN secret (long random string). Do not put it in git.
2. Commit and push main.cpp + te-backend.service to
   github.com/KeidranJr/taiwanese-enterprises.
3. On EC2: git pull in /opt/te, rebuild backend (-j1), fill ADMIN_TOKEN
   into the service file, systemctl daemon-reload, restart te-backend.
4. Check: plain GET /api/quotes answers 404; the curl above with the
   secret returns the list; the quote form still posts fine.

## Separate, same site: the phone number is still a placeholder

index.html line 170 shows (850) 555-0134 as the click-to-call number.
That is a 555 placeholder, not Keidran's business number. With a real
customer booked for Oct 24, anyone who taps "call" on the live site
reaches a fake number. Needs his real business number from him; swap and
redeploy after.
