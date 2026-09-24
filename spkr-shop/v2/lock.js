// The password page, in the same look as the site. `error` is fixed text from middleware.js, never user input.
export const lockPage = (error = '') => `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="robots" content="noindex">
<title>SPKR</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wdth,wght@62..125,100..900&family=Martian+Mono:wdth,wght@75..112.5,300..600&display=swap">
<style>
:root{
  --ground:#46494C;
  --smoke:rgba(129,141,146,.55); --umber:rgba(185,163,148,.38); --tan:rgba(212,197,199,.28); --stone:rgba(197,195,198,.20);
  --ink:#C5C3C6; --muted:#D4C5C7; --line:rgba(197,195,198,.22); --signal:#C5C3C6; --grain:.14;
  --display:"Archivo","Arial Narrow",Impact,sans-serif;
  --mono:"Martian Mono",ui-monospace,Consolas,monospace;
  --gutter:clamp(16px,3vw,40px);
  --ease:cubic-bezier(.2,.7,0,1);
  color-scheme:dark;
}
*{box-sizing:border-box}
html,body{height:100%}
body{
  margin:0; padding-inline:var(--gutter); overflow:hidden;
  background:
    radial-gradient(55% 72% at 90% 4%, var(--umber), transparent 70%),
    radial-gradient(45% 64% at 8% 0%, var(--stone), transparent 70%),
    radial-gradient(50% 67% at 2% 48%, var(--smoke), transparent 70%),
    radial-gradient(42% 61% at 96% 58%, var(--tan), transparent 70%),
    radial-gradient(30% 42% at 42% 96%, var(--umber), transparent 70%)
    var(--ground);
  color:var(--ink); font:400 15px/1.55 var(--display);
}
.grain{position:fixed; inset:-50%; pointer-events:none; opacity:var(--grain);
  background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='220' height='220'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='.9' numOctaves='3' stitchTiles='stitch'/%3E%3CfeColorMatrix values='0 0 0 0 .5 0 0 0 0 .5 0 0 0 0 .5 0 0 0 1.6 -.3'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)'/%3E%3C/svg%3E");
}
/* phones: finer, softer grain (high-density screens otherwise show each speck) */
@media (max-width:820px){ .grain{background-size:73px 73px; opacity:.1} }
.mono{font-family:var(--mono); font-size:10.5px; letter-spacing:.08em; text-transform:uppercase; font-stretch:87.5%}
.muted{color:var(--muted)}
.gate{position:relative; height:100%; display:grid; grid-template-rows:auto 1fr auto; padding-block:18px}
.top{display:flex; justify-content:space-between; gap:16px; padding-bottom:12px; border-bottom:1px solid var(--line)}
.wordmark{font-weight:900; font-stretch:62%; font-size:22px; letter-spacing:.02em; line-height:1}
.middle{align-self:center; display:grid; grid-template-columns:minmax(0,1.3fr) minmax(0,1fr); gap:var(--gutter); align-items:end}
.monument{margin:0 0 0 -.03em; font-weight:900; font-stretch:62%; line-height:.74; letter-spacing:-.035em; font-size:clamp(120px,25vw,380px);
  animation:settle 1.4s var(--ease) both}
@keyframes settle{from{letter-spacing:.12em; font-stretch:125%; opacity:0}}
form{display:flex; flex-direction:column; gap:14px; padding-bottom:1vw; animation:rise .9s var(--ease) .35s both}
@keyframes rise{from{opacity:0; transform:translateY(12px)}}
label{font-stretch:125%; font-weight:300; font-size:10.5px; letter-spacing:.32em; text-transform:uppercase}
.field{display:flex; border-bottom:1px solid var(--ink); transition:border-color .4s}
.field:focus-within{border-color:var(--signal)}
input{flex:1; min-width:0; background:none; border:0; color:var(--ink); padding:14px 0; font:500 22px var(--display); letter-spacing:.2em; outline:none}
button{background:none; border:0; color:var(--ink); cursor:pointer; padding:0 0 0 16px; transition:letter-spacing .4s var(--ease), color .3s}
button:hover{letter-spacing:.16em; color:var(--signal)}
button:focus-visible,input:focus-visible{outline:1px solid var(--signal); outline-offset:3px}
.error{color:var(--signal); margin:0}
.error:empty{display:none}
.shake{animation:shake .45s var(--ease)}
@keyframes shake{20%{transform:translateX(-8px)}45%{transform:translateX(6px)}70%{transform:translateX(-3px)}}
.bottom{display:flex; justify-content:space-between; gap:16px; padding-top:12px; border-top:1px solid var(--line)}
@media (max-width:700px){
  .middle{grid-template-columns:1fr; align-items:start}
  .monument{font-size:44vw}
}
@media (prefers-reduced-motion:reduce){ *{animation:none!important; transition:none!important} }
</style>
</head>
<body>
<div class="grain" aria-hidden="true"></div>
<main class="gate">
  <div class="top mono"><span class="wordmark">SPKR</span><span class="muted">Private preview</span></div>
  <div class="middle">
    <h1 class="monument" aria-label="SPKR">SPKR</h1>
    <form method="post" action="/unlock"${error ? ' class="shake"' : ''}>
      <label for="password">Password</label>
      <div class="field">
        <input id="password" name="password" type="password" autocomplete="current-password" required autofocus>
        <button class="mono" type="submit">Enter →</button>
      </div>
      <p class="mono error" role="alert">${error}</p>
      <p class="mono muted">SPKR.SHOP is not open yet.</p>
    </form>
  </div>
  <div class="bottom mono muted"><span>© 2026 SPKR</span><span>Software — Drum kits</span></div>
</main>
</body>
</html>`;
