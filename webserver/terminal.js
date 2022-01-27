const child_process = require("child_process");
const log = require('./log');

module.exports = function(command, args, verbose) {
    return new Promise((resolve, reject) => {
        const childProcess = child_process.exec(command, args);
        if (verbose === undefined || verbose) {
            log.write(command);
            childProcess.stdout.on("data", function(data) {
                for (let line of data.toString().split("\n")) {
                    log.write(line);
                }
            });
        }
        childProcess.stderr.on("data", function(data) {
            for (let line of data.toString().split("\n")) {
                log.write(line);
            }
        });
        childProcess.on("exit", function(code) {
            resolve(code);
        });
    })

}