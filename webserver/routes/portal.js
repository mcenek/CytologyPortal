let express = require('express');
let router = express.Router();

let render = require('../render');

router.get('/', function(req, res, next) {
  render('portal', {}, req, res, next);
});

module.exports = router;
