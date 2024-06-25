import fetch from "node-fetch";
import { execSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { arch, platform } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { x } from "tar";

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const pkg = JSON.parse(readFileSync(join(__dirname, "package.json"), "utf8"));

const GITHUB_REPO = "chase/node-shm-write";
const os = platform();
const arch_ = os === 'darwin' ? 'x64+arm64' : arch();
const url = `https://github.com/${GITHUB_REPO}/releases/download/v${pkg.version}/${os}-${arch_}.tar.gz`;

if (!existsSync(join(__dirname, "prebuilds", `${platform()}-${arch()}`))) {
  fetch(url)
    .then((res) => {
      if (!res.ok) throw new Error(`Failed to fetch ${url}`);
      return res.body.pipe(x({ C: __dirname }));
    })
    .catch((err) => {
      console.error(err);
      execSync("npm run prebuild");
    });
}
