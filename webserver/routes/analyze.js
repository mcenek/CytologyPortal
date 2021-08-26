const path = require('path');
const express = require('express');
const fileManager = require('../managers/fileManager');
const preferences = require('../preferences');
const multer = require("multer");
const multerStorage = require('../modules/multer/StorageEngine');
const terminal = require('../terminal');
const router = express.Router();


router.get('/', function(req, res, next) {
    res.render('analyze', {});
});

router.post('/', async function(req, res, next) {
    const files = preferences.get("files");

    const overwrite = true;
    return new Promise((resolve, reject) => {
        const uploadFile = async function (file) {
            let filePath = path.join(files, file.originalname, file.originalname);
            await fileManager.writeFile(filePath, file.stream, null, overwrite);
            terminal(`../analyzer/segment --image="${filePath}"`);
        }
        return multer({storage: multerStorage(uploadFile)}).any()(req, res, function(err) {
            if (err) res.status(500).send(res.locals.locale["upload_failed"]);
            else res.sendStatus(200);
            resolve();
        });
    });
});

module.exports = router;
