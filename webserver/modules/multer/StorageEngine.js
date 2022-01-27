const log = require("../../log");

class StorageEngine {
    #writeFile;

    constructor(writeFile) {
        this.#writeFile = writeFile;
    }

    async _handleFile(req, file, cb) {
        file.originalname = decodeURIComponent(file.originalname);
        try {
            await this.#writeFile(file);
        } catch (err) {
            log.writeServer(req, err);
            cb(err);
            return;
        }
        cb(null);
    }

    _removeFile = function _removeFile(req, file, cb) {
        cb(null);
    }
}

module.exports = function(writeFile) {
    return new StorageEngine(writeFile);
}
