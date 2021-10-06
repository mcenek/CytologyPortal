let express = require('express');
const readify = require('readify');
const router = express.Router();
const filesRouter = require('../routes/files');
const path = require('path');
const preferences = require('../preferences');

const getRelativeDirectory = async req => preferences.get("files");
const getFilePath = async req => path.join(await getRelativeDirectory(req), decodeURIComponent(req.path));

router.get('/:image', async function(req, res, next) {
    const parameter = Object.keys(req.query)[0];
    if (parameter) return next();
    const image = req.params.image;
    if (image.startsWith(".recycle")) return next();
    try {
        res.render('portal', {image: image, preferences: preferences.get("portal")});
    } catch {
        return next();
    }
});

router.use(filesRouter(getRelativeDirectory, getFilePath))

module.exports = router;
