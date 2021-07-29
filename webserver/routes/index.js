let express = require('express');
let router = express.Router();

let render = require('../render');

router.get('/', function(req, res, next) {
  res.redirect("/portal");
});

module.exports = router;
