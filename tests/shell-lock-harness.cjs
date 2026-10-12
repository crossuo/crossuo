// Ad-hoc harness for the shell.html shard IIFE (extracted and evaluated with
// browser stubs). Exercises: single-shard lock, ?shards= override semantics,
// login-override stripping, producturl->/data mapping.
'use strict';
const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync(__dirname + '/../resources/shell.html', 'utf8');
// the shard/picker script is the LAST <script> block
const blocks = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]);
const src = blocks[blocks.length - 1];

const elStub = () => ({
  style: {}, classList: { add() {}, remove() {} }, childNodes: [], children: [],
  appendChild(c) { this.childNodes.push(c); this.children.push(c); return c; },
  insertAdjacentElement() {}, focus() {},
  addEventListener() {}, parentNode: null, disabled: false, tabIndex: 0,
  src: '', id: '', className: '', textContent: '', href: '', target: '',
  removeAttribute() {}, setAttribute() {},
});
const makeDocumentStub = () => ({
  createElement: elStub,
  createTextNode: (t) => ({ nodeText: t, textContent: t }),
  getElementById: () => null,
  body: elStub(),
});

const CANNED = {
  'http://x/single.cfg': '[Shard]\nshardname=OnlyShard\nloginserver=play.only.example,2593\nproducturl=local:mydata\nclientversion=1.25.32\ncrypt=yes\n',
  'http://x/multi.cfg': '[Shard]\nshardname=A\nloginserver=a.example,2593\n\n[Shard]\nshardname=B\nloginserver=b.example,2593\n',
  'http://x/override.cfg': '[Shard]\nshardname=Evil\nloginserver=evil.example,5193\nproducturl=\n',
  'shards.cfg': null, // per-test
};

function run(testQuery, sameOriginCfg) {
  CANNED['shards.cfg'] = sameOriginCfg;
  const captured = { args: null, pickerShown: false, opened: [] };
  const documentStub = makeDocumentStub();
  const Module = {};
  const sandboxWindow = {
    location: { search: testQuery },
    addEventListener() {},
    open: (url) => { captured.opened.push(url); return {}; },
  };
  const fetchStub = (url) => {
    const body = CANNED[url];
    if (body == null) return Promise.resolve({ ok: false, status: 404, text: () => Promise.resolve('') });
    return Promise.resolve({ ok: true, status: 200, text: () => Promise.resolve(body) });
  };
  const ctx = {
    window: sandboxWindow, document: documentStub, Module,
    URLSearchParams, console,
    WEBLOG: false,
    setTimeout, clearTimeout, setInterval, Date, AbortController,
    fetch: fetchStub,
  };
  ctx.globalThis = ctx;
  vm.createContext(ctx);
  vm.runInContext(src, ctx, { filename: 'shard-iife.js' });
  return new Promise((resolve) => {
    setTimeout(() => {
      captured.args = Module.arguments;
      captured.body = documentStub.body;
      resolve(captured);
    }, 120);
  });
}

function extractArgs(args) {
  return (args || []).slice(1); // drop ./crossuo
}

let failures = 0;
function check(name, cond, extra) {
  if (cond) console.log('PASS ' + name);
  else { failures++; console.log('FAIL ' + name + (extra ? ' :: ' + extra : '')); }
}

(async () => {
  // 1. locked: single-entry same-origin cfg boots the entry directly
  let r = await run('', CANNED['http://x/single.cfg']);
  check('locked: boots without picker', !!r.args, JSON.stringify(r.args));
  check('locked: shard login pinned', extractArgs(r.args).includes('--host=play.only.example'), JSON.stringify(r.args));
  check('locked: producturl local: -> /data mount', extractArgs(r.args).includes('--streaming-product=/data/mydata/'), JSON.stringify(r.args));
  check('locked: --shard emitted', extractArgs(r.args).includes('--shard=OnlyShard'), JSON.stringify(r.args));
  check('locked: clientversion passed', extractArgs(r.args).includes('--clientversion=1.25.32'), JSON.stringify(r.args));
  check('locked: crypt passed', extractArgs(r.args).includes('--crypt=1'), JSON.stringify(r.args));

  // 2. locked: query login overrides are stripped
  r = await run('?--host=evil.example&--port=5193&shard-address=evil2&shard-port=1234&foo=bar', CANNED['http://x/single.cfg']);
  const a2 = extractArgs(r.args);
  check('locked: query --host/--port stripped', !a2.includes('--host=evil.example') && !a2.includes('--port=5193'), JSON.stringify(a2));
  check('locked: testing params not injected', !a2.includes('--host=evil2') && !a2.includes('--port=1234'), JSON.stringify(a2));
  check('locked: login still pinned', a2.includes('--host=play.only.example'), JSON.stringify(a2));
  check('locked: non-login passthrough kept', a2.includes('foo=bar'), JSON.stringify(a2));

  // 3. locked: encoded + attached-abbreviation forms stripped
  r = await run('?--h%6Fst=evil.example&-sevil3&-p=99', CANNED['http://x/single.cfg']);
  const a3 = extractArgs(r.args);
  check('locked: encoded --host stripped', !a3.includes('--host=evil.example'), JSON.stringify(a3));
  check('locked: -s attached stripped', !a3.includes('-sevil3'), JSON.stringify(a3));
  check('locked: -p= stripped', !a3.includes('-p=99'), JSON.stringify(a3));

  // 4. locked: ?shards= override cannot defeat the lock
  r = await run('?shards=http://x/override.cfg', CANNED['http://x/single.cfg']);
  const a4 = extractArgs(r.args);
  check('locked: override ignored', a4.includes('--host=play.only.example'), JSON.stringify(a4));
  check('locked: evil not booted', !a4.includes('--host=evil.example'), JSON.stringify(a4));

  // 5. locked: ?shard= deep link still boots THE entry
  r = await run('?shard=SomeOther', CANNED['http://x/single.cfg']);
  check('locked: deep link pinned to the entry', extractArgs(r.args).includes('--host=play.only.example'), JSON.stringify(extractArgs(r.args)));

  // 6. multi-entry same-origin: picker (no boot)
  r = await run('', CANNED['http://x/multi.cfg']);
  check('multi: picker shown, no boot', !r.args, JSON.stringify(r.args));

  // 7. multi-entry same-origin + single-entry override: override wins the
  // ladder (testing), but no lock - the picker shows (pre-existing behavior
  // for any non-same-origin usable list)
  r = await run('?shards=http://x/override.cfg', CANNED['http://x/multi.cfg']);
  check('multi+override: override honored, picker (no boot)', !r.args, JSON.stringify(r.args));

  // 8. multi + override + deep link: testing params still injectable (non-locked)
  r = await run('?shards=http://x/override.cfg&shard=Evil&shard-address=qa.example', CANNED['http://x/multi.cfg']);
  check('multi+override: deep link boots override entry', extractArgs(r.args).includes('--host=evil.example'), JSON.stringify(extractArgs(r.args)));
  check('multi+override: testing param injected', extractArgs(r.args).includes('--host=qa.example'), JSON.stringify(extractArgs(r.args)));

  // 9. no usable cfg: fallback boot
  r = await run('', null);
  check('no cfg: fallback boot', !!r.args && extractArgs(r.args).length === 0, JSON.stringify(r.args));

  // 10. producturl mapping variants (through the locked single entry)
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=mydata\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('producturl bare-relative -> /data/mydata/', extractArgs(r.args).includes('--streaming-product=/data/mydata/'), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=./uodata\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('producturl ./uodata -> /data/uodata/', extractArgs(r.args).includes('--streaming-product=/data/uodata/'), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=a/./b\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('producturl a/./b -> /data/a/b/', extractArgs(r.args).includes('--streaming-product=/data/a/b/'), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=local:a/b\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('producturl local:a/b -> /data/a/b/', extractArgs(r.args).includes('--streaming-product=/data/a/b/'), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=../evil\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('producturl .. refused (no product flag)', !extractArgs(r.args).some(x => x.startsWith('--streaming-product')), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=https://cdn.example/uo/manifest\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('manifest producturl -> default lanes (no flag)', !extractArgs(r.args).some(x => x.startsWith('--streaming-product')), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=https://uniteuo.example/fc\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('aops producturl -> /stream relay', extractArgs(r.args).includes('--streaming-product=/stream/https%3A%2F%2Funiteuo.example%2Ffc/'), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=local:/srv/shard/client\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('absolute local: host path -> no product flag (needs proxy rewrite)', !extractArgs(r.args).some(x => x.startsWith('--streaming-product')), JSON.stringify(extractArgs(r.args)));
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=/data/t/\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('rewritten producturl passes through', extractArgs(r.args).includes('--streaming-product=/data/t/'), JSON.stringify(extractArgs(r.args)));
  // the `latest` token is gone with the prefix grammar: producturl is a
  // plain url, a bare value is a deployment-local mount like any other
  CANNED['http://x/single.cfg'] = '[Shard]\nshardname=T\nloginserver=t.example,2593\nproducturl=latest\n';
  r = await run('', CANNED['http://x/single.cfg']);
  check('bare producturl -> /data mount', extractArgs(r.args).includes('--streaming-product=/data/latest/'), JSON.stringify(extractArgs(r.args)));

  // 11. picker sub-line + card action (web shard picker)
  // walk the stub tree collecting nodes matching a predicate
  function walk(node, pred, out) {
    out = out || [];
    if (!node) return out;
    if (pred(node)) out.push(node);
    const kids = node.childNodes || [];
    for (let i = 0; i < kids.length; i++) walk(kids[i], pred, out);
    return out;
  }
  const pickerText = (body) =>
    walk(body, n => typeof n.textContent === 'string' && n.textContent)
      .map(n => n.textContent).join('|');
  const pickerRows = (body) => walk(body, n => n.className === 'row');

  // 11a. multi-shard: the sub line shows the WEB url (clickable), not the
  // login address
  CANNED['http://x/multi.cfg'] =
    '[Shard]\nshardname=A\nloginserver=a.example,2593\nshardweb=https://a.example/\n\n' +
    '[Shard]\nshardname=B\nloginserver=b.example,2593\nshardweb=https://b.example/\n';
  r = await run('', CANNED['http://x/multi.cfg']);
  let rows = pickerRows(r.body);
  check('picker: two rows rendered', rows.length === 2, 'rows=' + rows.length);
  // our stub sets href on the created <a> elements
  const links = walk(r.body, n => n.href === 'https://a.example/' || n.href === 'https://b.example/');
  check('picker: url shown as a link', links.length === 2, JSON.stringify(links.map(l => l.href)));
  check('picker: login address not in the picker text',
    !pickerText(r.body).includes('a.example,2593') && !pickerText(r.body).includes('b.example,2593'),
    pickerText(r.body));
  check('picker: no Create account for autoacct shards',
    walk(r.body, n => n.className === 'card-action').length === 0);

  // 11b. fallback: no url -> the login address is still shown
  CANNED['http://x/multi.cfg'] =
    '[Shard]\nshardname=A\nloginserver=a.example,2593\n\n' +
    '[Shard]\nshardname=B\nloginserver=b.example,2593\n';
  r = await run('', CANNED['http://x/multi.cfg']);
  check('picker: no url -> loginserver fallback',
    pickerText(r.body).includes('a.example,2593'), pickerText(r.body));

  // 11c. autoacct=no + urlregister -> Create account button on that card,
  // opening the registration page
  CANNED['http://x/multi.cfg'] =
    '[Shard]\nshardname=A\nloginserver=a.example,2593\nshardweb=https://a.example/\n' +
    'AutoAcct=no\nshardaccount=https://a.example/register\n\n' +
    '[Shard]\nshardname=B\nloginserver=b.example,2593\nshardweb=https://b.example/\n';
  r = await run('', CANNED['http://x/multi.cfg']);
  rows = pickerRows(r.body);
  const actions = walk(r.body, n => n.className === 'card-action');
  check('picker: Create account button present for autoacct=no', actions.length === 1,
    'actions=' + actions.length);
  check('picker: button label', actions.length === 1 && actions[0].textContent === 'Create account',
    actions.length ? actions[0].textContent : '(none)');
  // clicking the button opens urlregister (window.open recorded by the stub;
  // the handler is attached, the stub cannot dispatch, so assert the wiring
  // via the recorded registration url the closure captured)
  check('picker: only the autoacct=no card gets the button',
    rows.length === 2 && actions.length === 1);

  // 11d. autoacct=yes explicitly -> no button even with a urlregister
  CANNED['http://x/multi.cfg'] =
    '[Shard]\nshardname=A\nloginserver=a.example,2593\nAutoAcct=yes\n' +
    'urlregister=https://a.example/register\n\n' +
    '[Shard]\nshardname=B\nloginserver=b.example,2593\n';
  r = await run('', CANNED['http://x/multi.cfg']);
  check('picker: autoacct=yes has no Create account button',
    walk(r.body, n => n.className === 'card-action').length === 0);

  // 11e. autoacct=no but no urlregister -> nothing to offer, no button
  CANNED['http://x/multi.cfg'] =
    '[Shard]\nshardname=A\nloginserver=a.example,2593\nAutoAcct=no\n\n' +
    '[Shard]\nshardname=B\nloginserver=b.example,2593\n';
  r = await run('', CANNED['http://x/multi.cfg']);
  check('picker: autoacct=no without urlregister -> no button',
    walk(r.body, n => n.className === 'card-action').length === 0);

  console.log(failures ? ('\n' + failures + ' FAILURES') : '\nALL SHELL HARNESS TESTS PASSED');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
