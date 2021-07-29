const localeManager = require("./managers/localeManager");
const os = require("os");
const path = require("path");

async function render(view, args, req, res, next) {
    res.render(view, Object.assign({
        hostname: os.hostname(),
        locale: localeManager.get(req)
    }, args));
}

module.exports = render;