const { platform, arch } = require("node:os");
const { existsSync, createWriteStream, unlinkSync } = require("node:fs");
const { execSync } = require("node:child_process");
const path = require("node:path");
const https = require("node:https");
const tar = require("tar");
const package = require("./package.json");

const GITHUB_REPO = "chase/node-shm-write";
const version = package.version;
const url = `https://github.com/${GITHUB_REPO}/releases/download/v${version}/${platform()}-${arch()}.tar.gz`;

function downloadAndExtract(url, dest, cb) {
  const file = path.join(dest, "archive.tar.gz");
  const fileStream = createWriteStream(file);
  https
    .get(url, (response) => {
      if (response.statusCode !== 200) {
        cb(new Error(`Failed to fetch ${url}`));
        return;
      }
      response.pipe(fileStream);
      fileStream.on("finish", () => {
        fileStream.close(() => {
          tar
            .x({ file, C: dest })
            .then(() => {
              unlinkSync(file);
              cb();
            })
            .catch(() => {
              unlinkSync(file);
              cb();
            });
        });
      });
    })
    .on("error", cb);
}

if (
  !existsSync(
    path.join(__dirname, "prebuilds", `${platform()}-${arch()}`),
  )
) {
  downloadAndExtract(url, __dirname, (err) => {
    if (err) {
      console.error(err);
      execSync("npm run prebuild");
    }
  });
}
