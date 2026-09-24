// Password gate for the whole site. Runs on Vercel before any file is served.
// The password lives in the SITE_PASSWORD environment variable (Vercel → Project → Settings → Environment Variables).
import { next } from '@vercel/functions';
import { lockPage } from './lock.js';

export const config = { matcher: '/:path*' };

const COOKIE = 'spkr_access';
const THIRTY_DAYS = 60 * 60 * 24 * 30;

// The cookie holds a hash of the password, never the password itself; changing the password logs everyone out.
async function tokenFor(password) {
  const bytes = new TextEncoder().encode('spkr-gate:' + password);
  const hash = await crypto.subtle.digest('SHA-256', bytes);
  return [...new Uint8Array(hash)].map(b => b.toString(16).padStart(2, '0')).join('');
}

const page = (error, status) => new Response(lockPage(error), {
  status,
  headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'no-store' },
});

export default async function middleware(request) {
  // Tolerate stray spaces or quotes pasted into the Vercel setting.
  const password = (process.env.SITE_PASSWORD ?? '').trim().replace(/^(["'])(.*)\1$/, '$2');
  if (!password) {
    // Fail closed: without a configured password nobody gets in.
    console.error('SITE_PASSWORD is not set; the site stays locked.');
    return page('', 503);
  }
  const token = await tokenFor(password);
  const url = new URL(request.url);

  if (url.pathname === '/unlock' && request.method === 'POST') {
    const form = await request.formData();
    if (await tokenFor(String(form.get('password') ?? '').trim()) !== token) return page('That password isn’t right. Try again.', 401);
    return new Response(null, {
      status: 303,
      headers: {
        location: '/',
        'set-cookie': `${COOKIE}=${token}; Path=/; Max-Age=${THIRTY_DAYS}; HttpOnly; Secure; SameSite=Lax`,
      },
    });
  }

  const cookies = (request.headers.get('cookie') ?? '').split(/;\s*/);
  if (cookies.includes(`${COOKIE}=${token}`)) return next();
  return page('', 401);
}
