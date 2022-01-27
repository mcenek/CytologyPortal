let express = require('express');
let router = express.Router();

router.get('/', function(req, res, next) {
  res.redirect("/portal");
});

module.exports = router;
