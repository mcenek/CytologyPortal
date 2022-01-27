const archiver = require('archiver');
const fs = require("fs");
const mkdirp = require("mkdirp")
const multer = require("multer")
const multerStorage = require('../modules/multer/StorageEngine');
const path = require("path");
const mime = require("mime")
const tmp = require('tmp-promise');
const createError = require("http-errors");
const localeManager = require("./localeManager");
const log = require("../log");
const readify = require("readify");

async function createArchive(directories) {
    const archive = initArchive();
    if (typeof directories === "string") directories = [directories];

    async function callback(i) {
        if (directories.hasOwnProperty(i)) {
            await walkDirectoryPreorder(directories[i], async function (filePath, isDirectory) {
                if (isDirectory) return;
                const readStream = await readFile(filePath);
                const name = path.relative(path.dirname(directories[i]), filePath);
                archive.append(readStream, {name: name});
            });
            return await callback(i + 1);
        } else {
            archive.finalize();
            return archive;
        }
    }

    return await callback(0);
}

function downloadFile(fileStream, fileName, req, res, next) {
    let header = {
        "Content-Type": "application/octet-stream",
        "Content-Disposition": "attachment"
    };
    if (fileName) {
        header["Content-Disposition"] += `; filename="${path.basename(fileName)}"`;
    }
    res.writeHead(200, header);
    fileStream.pipe(res);
}

function downloadFolder(archiveStream, fileName, req, res, next) {
    if (!fileName) fileName = localeManager.get(req).app_name;
    fileName = `${path.basename(fileName)}-${Math.floor(Date.now() / 1000)}.zip`;
    res.writeHead(200,
        {
            "Content-Disposition": `attachment; filename="${path.basename(fileName)}"`,
            "Content-Type": "application/octet-stream"
        });
    archiveStream.pipe(res);
}

async function deleteFile(filePath, relativeDirectory) {
    let recycleDirectory = path.join(relativeDirectory, ".recycle");
    let deletedPath = path.join(recycleDirectory, path.relative(relativeDirectory, filePath));

    if (filePath.startsWith(recycleDirectory)) {
        try {
            await fs.promises.unlink(filePath);
        } catch (err) {
            log.write(err);
            return Promise.reject(err);
        }
    } else {
        try {
            await mkdirp(path.dirname(deletedPath));
            await fs.promises.rename(filePath, deletedPath);
        } catch (err) {
            log.write(err);
            return Promise.reject(err);
        }
    }
}

function initArchive() {
    let archive = archiver('zip');
    archive.on('error', function (err) {
        log.write(err);
    });
    return archive;
}

function inlineFile(fileStream, fileName, req, res, next) {
    let header = {
        "Content-Disposition": "inline",
        "Content-Type": mime.getType(fileName)
    };
    if (fileName) {
        header["Content-Disposition"] += `; filename="${encodeURIComponent(path.basename(fileName))}"`;
    }
    res.writeHead(200, header);
    fileStream.pipe(res);
}


function processUpload(saveLocation, overwrite, req, res, next) {
    const locale = localeManager.get(req);
    return new Promise((resolve, reject) => {
        const uploadFile = async function (file) {
            let filePath = path.join(saveLocation, file.originalname);
            await writeFile(filePath, file.stream, null, overwrite);
        }
        return multer({storage: multerStorage(uploadFile)}).any()(req, res, function (err) {
            if (err) res.status(500).send(locale.upload_failed);
            else if (Object.keys(req.files).length === 1) res.send(locale.uploaded_file);
            else res.send(locale.uploaded_files);
            resolve();
        });
    })
}

async function readDirectory(directory, eachFile) {
    try {
        const files = await fs.promises.readdir(directory);

        async function nextFile(i) {
            if (files.hasOwnProperty(i)) {
                let file = files[i];
                let filePath = path.join(directory, file);
                try {
                    const stats = await fs.promises.stat(filePath);
                    await eachFile(filePath, stats.isDirectory());
                    await nextFile(i + 1);
                } catch (err) {
                    log.write(err);
                    await nextFile(i + 1);
                }
            }
        }

        await nextFile(0);
    } catch (err) {
        log.write(err);
        return Promise.reject(err);
    }
}

async function readFile(filePath, options) {
    const readStream = fs.createReadStream(filePath, options);
    readStream.on("error", function (err) {
        log.write(err);
    });
    return readStream;
}

async function renderDirectory(directory, relativeDirectory, req, res, next) {
    try {
        const data = await readify(directory, {sort: 'type'});
        directory = path.relative(relativeDirectory, directory)
        const re = new RegExp("\\" + path.sep, "g");
        directory = directory.replace(re, "/");
        await res.render("directory", {files: data.files, path: directory, baseUrl: req.baseUrl});
    } catch (err) {
        log.write(err);
        next(createError(404))
    }
}

async function renderFile(displayName, req, res, next) {
    return await res.render("fileViewer", {displayName: encodeURIComponent(displayName)});
}

async function walkDirectoryPreorder(directory, eachFile) {
    await readDirectory(directory, async function (filePath, isDirectory) {
        const newDirectoryName = await eachFile(filePath, isDirectory);
        if (isDirectory) {
            if (newDirectoryName) filePath = newDirectoryName;
            await walkDirectoryPreorder(filePath, eachFile);
        }
    });
}

async function walkDirectoryPostorder(directory, eachFile) {
    await readDirectory(directory, async function (filePath, isDirectory) {
        if (isDirectory) {
            await walkDirectoryPostorder(filePath, eachFile);
        }
        await eachFile(filePath, isDirectory);
    });
}

async function writeFile(filePath, contentStream, prependBuffer, overwrite) {
    const tmpFilePath = await newTmpFile();

    const writeStream = fs.createWriteStream(tmpFilePath);

    if (prependBuffer) {
        for (const buffer of prependBuffer) {
            writeStream.write(buffer);
        }
    }

    contentStream.on("data", function (data) {
        writeStream.write(data);
    })

    contentStream.on("end", function () {
        writeStream.close();
    })

    contentStream.on("error", async function (err) {
        log.write(err);
        try {
            await fs.promises.unlink(tmpFilePath)
        } catch {
        }
    });

    writeStream.on("close", async function () {
        await mkdirp(path.dirname(filePath));
        try {
            await fs.promises.stat(filePath);
        } catch {
            await fs.promises.rename(tmpFilePath, filePath);
            return;
        }
        const newFilePath = overwrite ? filePath : await findSimilarName(filePath);
        await fs.promises.rename(tmpFilePath, newFilePath);
    });


}

async function findSimilarName(filePath, counter) {
    if (!counter) counter = 2;
    const parent = path.dirname(filePath);
    const fileBasename = path.basename(filePath);
    let newFileBaseName;
    if (filePath.includes(".")) {
        const fileNameSplit = fileBasename.split(".");
        const extension = fileNameSplit.pop();
        const fileName = fileNameSplit.join(".");
        newFileBaseName = `${fileName}-${counter}.${extension}`;
    } else {
        newFileBaseName = `${fileBasename}-${counter}`;
    }

    const newFilePath = path.join(parent, newFileBaseName)
    try {
        await fs.promises.stat(newFilePath);
    } catch {
        return newFilePath;
    }
    return await findSimilarName(filePath, counter + 1)
}

async function newTmpFile(directory) {
    if (!directory) directory = "./tmp";
    await mkdirp(directory);
    try {
        return tmp.tmpName({tmpdir: directory});
    } catch (err) {
        log.write(err);
        return Promise.reject(err);
    }
}

module.exports = {
    createArchive: createArchive,
    deleteFile: deleteFile,
    downloadFile: downloadFile,
    downloadFolder: downloadFolder,
    initArchive: initArchive,
    inlineFile: inlineFile,
    processUpload: processUpload,
    readDirectory: readDirectory,
    readFile: readFile,
    renderDirectory: renderDirectory,
    renderFile: renderFile,
    walkDirectoryPreorder: walkDirectoryPreorder,
    walkDirectoryPostorder: walkDirectoryPostorder,
    writeFile: writeFile,
};