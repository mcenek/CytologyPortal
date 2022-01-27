const express = require('express');
const preferences = require('../preferences');

const router = express.Router();

router.get('/', function(req, res, next) {
    res.render('preferences', {preferences: preferences.get("portal")});
});

router.post('/*', async function(req, res, next) {
    let portalPreferences = preferences.get("portal");
    for (const key in req.body) {
        portalPreferences[key] = req.body[key];
    }
    let updated = preferences.set("portal", portalPreferences);
    if (updated) res.sendStatus(200);
    else res.sendStatus(400);
});

module.exports = router;
