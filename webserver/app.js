let createError = require('http-errors');
let express = require('express');
let path = require('path');
let cookieParser = require('cookie-parser');
let preferences = require("./preferences");
let localeManager = require("./managers/localeManager");
let morgan = require('morgan');

const routes = {
    "/": require('./routes/index'),
    "/analyze": require('./routes/analyze'),
    "/portal": require('./routes/portal'),
}

let app = express();

app.use(function (req, res, next) {
    if (req.originalUrl === "/") return next();
    let urlCleanup = req.originalUrl
        .replace(/\/{2,}/g, "/")
        .replace(/\/$/, "");
    if (req.originalUrl !== urlCleanup) {
        return res.redirect(urlCleanup);
    }
    next();
});

app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'pug');

app.use(morgan('dev'));
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(cookieParser());
app.use(express.static(path.join(__dirname, 'public')));
app.use(localeManager.getHandler)

for (const route in routes) {
    app.use(route, routes[route]);
}

// catch 404 and forward to error handler
app.use(function(req, res, next) {
  next(createError(404));
});

// error handler
app.use(function(err, req, res, next) {
  // set locals, only providing error in development
  res.locals.message = err.message;
  res.locals.error = req.app.get('env') === 'development' ? err : {};

  // render the error page
  res.status(err.status || 500);
  res.render('error', {}, req, res, next);
});

module.exports = app;
