// Optional browser check. Install playwright-core and @sparticuz/chromium outside source.
const {chromium: playwright} = require('playwright-core');
const chromiumModule = require('@sparticuz/chromium');
const chromium = chromiumModule.default || chromiumModule;
const {spawn} = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '..');
(async () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'stockguard-browser-'));
  const server = spawn('python3', [path.join(root, 'dashboard/server.py'), '--db', path.join(directory, 'demo.db'), '--port', '0'], {cwd: root});
  let browser;
  try {
    const port = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Server startup timeout')), 15000);
      let output = '';
      server.stdout.on('data', chunk => {
        output += chunk;
        const match = output.match(/listening on port (\d+)/);
        if (match) {clearTimeout(timer); resolve(Number(match[1]));}
      });
      server.once('exit', code => {clearTimeout(timer); reject(new Error(`Server exited ${code}`));});
    });
    browser = await playwright.launch({args: ["--no-sandbox", "--disable-dev-shm-usage", "--disable-gpu"], executablePath: await chromium.executablePath(), headless: true});
    const page = await browser.newPage({viewport: {width: 1280, height: 1000}});
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.goto(`http://127.0.0.1:${port}`, {waitUntil: 'networkidle'});
    await page.locator('[data-scenario="consistent"]').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"status": "ACCEPT"'));
    await page.locator('#replay').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"replayed": true'));
    await page.locator('[data-scenario="low"]').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"status": "REVIEW"'));
    await page.waitForSelector('#pending table tr:nth-child(2)');
    const id = await page.locator('#pending table tr').nth(1).locator('td').nth(0).innerText();
    const headers = await page.locator('#inventory table th').allTextContents();
    const version = await page.locator('#inventory table tr').nth(1).locator('td').nth(headers.indexOf('version')).innerText();
    await page.locator('#review [name="observation_id"]').fill(id);
    await page.locator('#review [name="expected_version"]').fill(version);
    await page.locator('#review [name="quantity"]').fill('99');
    await page.locator('#review button').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"action": "correct"'));
    await page.waitForFunction(() => document.getElementById('inventory').textContent.includes('99') && document.getElementById('pending').textContent === 'No records.');
    assert.equal(await page.locator('#message').innerText(), '');
    assert.equal(await page.locator('#audit table tr').count(), 4);
    await page.locator('[data-scenario="partial"]').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"status": "DEFER"'));
    await page.waitForSelector('#pending table tr:nth-child(2)');
    const deferredId = await page.locator('#pending table tr').nth(1).locator('td').nth(0).innerText();
    await page.locator('#review [name="observation_id"]').fill(deferredId);
    await page.locator('#review [name="expected_version"]').fill('');
    await page.locator('#review [name="action"]').selectOption('reject');
    assert.equal(await page.locator('#review [name="expected_version"]').isDisabled(), true);
    await page.locator('#review button').click();
    await page.waitForFunction(() => document.getElementById('decision').textContent.includes('"action": "reject"') && document.getElementById('pending').textContent === 'No records.');
    assert.equal(await page.locator('#audit table tr').count(), 4);
    assert.equal(await page.locator('#message').innerText(), '');
    assert.deepEqual(errors, []);
    fs.mkdirSync(path.join(root, 'docs/images'), {recursive: true});
    await page.screenshot({path: path.join(root, 'docs/images/dashboard.png'), fullPage: true});
    console.log('Browser smoke passed: consistent scan, replay, review correction, rejection without version and visible audit');
  } finally {
    if (browser) await browser.close();
    const exited = new Promise(resolve => server.once('exit', resolve));
    server.kill(); await exited;
    fs.rmSync(directory, {recursive: true, force: true});
  }
})().catch(error => {console.error(error); process.exitCode = 1;});
