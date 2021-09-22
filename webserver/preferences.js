const fs = require('fs');
const log = require('./log');

const configurationFile = "preferences.json";

let defaultConfiguration = {
    files: "../images",
    port: 443,
    httpRedirectServer: false,
    keys: {
        https: {
            cert: "./keys/https/cert.crt",
            key: "./keys/https/key.key",
        },
    }
};

let configuration;

function cleanup() {
    let modified = false;
    for (let property in configuration) {
        if (configuration.hasOwnProperty(property) && !defaultConfiguration.hasOwnProperty(property)) {
            delete configuration[property];
            modified = true;
            log.write("Deleted unused configuration value: " + property);
        }
    }
    if (modified) {
        save();
    }
}

function get(property) {
    if (configuration) {
        if (!configuration.hasOwnProperty(property)) {
            if (defaultConfiguration.hasOwnProperty(property)) {
                configuration[property] = defaultConfiguration[property];
                save();
            } else {
                log.write("Property '" + property + "' does not exist")
            }
        }
        return configuration[property];
    }
}

function init() {
    let data;
    try {
        data = fs.readFileSync(configurationFile);
    } catch {
        writeJSON(configurationFile, defaultConfiguration);
        return init();
    }
    data = data.toString().trim();
    log.write(`Reading ${configurationFile}...`);
    try {
        configuration = JSON.parse(data);
        cleanup();
    } catch (err) {
        log.write("Read error: " + err);
        configuration = defaultConfiguration;
        save();
    }
}

function save() {
    writeJSON(configurationFile, configuration);
}

function setDefaultConfiguration(configuration) {
    Object.assign(defaultConfiguration, configuration);
}

function writeJSON(filePath, json) {
    fs.writeFileSync(filePath, JSON.stringify(json, null, 4));
}

module.exports = {
    get: get,
    init: init,
    setDefaultConfiguration: setDefaultConfiguration
};