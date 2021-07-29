const locale = require("locale");
const fs = require("fs");
const path = require("path");
const log = require("../log");

const extension = ".json";

const localeDirectory = path.join(".", "locales");
let defaultLocale = "en_US";

let localeManager = {};
let handler;
let hasInit = false;

if (!hasInit) init();

function init() {
    log.write("Loading locales...")
    load(localeDirectory);

    if (!localeManager.hasOwnProperty(defaultLocale)) defaultLocale = localeManager[0];

    handler = locale(Object.keys(localeManager), defaultLocale)
}


function load(directory) {
    let localeFiles = fs.readdirSync(directory)
    for (let localeFile of localeFiles) {
        if (localeFile.endsWith(extension)) {
            let localeName = localeFile.substring(0, localeFile.length - extension.length);
            try {
                let localeData = fs.readFileSync(path.join(directory, localeFile));
                localeData = localeData.toString();
                localeData = JSON.parse(localeData)
                if (localeManager[localeName]) {
                    Object.assign(localeManager[localeName], localeManager[localeName], localeData);
                } else localeManager[localeName] = localeData;
            } catch {
                log.write(`Invalid locale file: ${localeName}`);
            }
        }
    }
    if (localeManager.length === 0) {
        throw new Error("No locales found")
    }
}

function get(req) {
    let sessionLocale = defaultLocale;

    if (req) {
        sessionLocale = req.locale;
        let cookieLocale = req.cookies.locale;
        if (cookieLocale && localeManager.hasOwnProperty(cookieLocale)) {
            sessionLocale = cookieLocale;
        }
    }

    let localeData = localeManager[sessionLocale];
    if (sessionLocale !== defaultLocale) {
        let defaultLocaleData = localeManager[defaultLocale];
        localeData = Object.assign({}, defaultLocaleData, localeData);
    }
    return localeData;
}

function getHandler() {
    return handler;
}

function isSupported(locale) {
    return localeManager.hasOwnProperty(locale);
}

module.exports = {
    init: init,
    get: get,
    getHandler: getHandler,
    isSupported: isSupported,
}