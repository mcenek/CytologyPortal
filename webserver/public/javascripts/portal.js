
let nucleiCytoRatios;
let thumbnails;
let sorted;
let page = 0;
let shownThumbnails;
let shownThumbnailsIdx;
const thumbnailsPerPage = preferences.thumbnails_per_page;

$(document).ready(async () => {
    let exportXhr = await getRequest(location.pathname + "/export.json");
    let logXhr = await getRequest(location.pathname + "/log.txt");

    parseData(JSON.parse(exportXhr.responseText))
    showChart();
    $("#info").append(logXhr.responseText.trim())

    initThumbnails();
    $("#show-thumbnails").click(initThumbnails);
    $("#thumbnails-prev").click(showPreviousThumbnails);
    $("#thumbnails-next").click(showNextThumbnails);

    setTimeout(() => $("#loading").hide(), 500)

})

function showPreviousThumbnails() {
    if (page <= 0) return;
    page--;
    displayThumbnails(page);
}

function showNextThumbnails() {
    if (page + 1 >= shownThumbnails.length / thumbnailsPerPage) return;
    page++;
    displayThumbnails(page);
}

function initThumbnails() {
    $("#thumbnails-card").show();
    page = 0;
    shownThumbnails = [];
    shownThumbnailsIdx = [];
    for (const i in thumbnails) {

        const ratio = nucleiCytoRatios[sorted[i]];
        if (ratio >= parseFloat($("#lower-ratio").val()) && ratio <= parseFloat($("#upper-ratio").val())) {
            shownThumbnails.push(thumbnails[sorted[i]]);
            shownThumbnailsIdx.push(i);
        }
    }

    let thumbnailMin = thumbnails[sorted[shownThumbnailsIdx[0]]];
    let thumbnailMax = thumbnails[sorted[shownThumbnailsIdx[shownThumbnailsIdx.length - 1]]];
    $("#lower-bound-thumbnail").html(`
                <img class="thumbnail" height=50px src="${location.pathname + "/thumbnails/" + thumbnailMin}" loading="lazy">               
            `)

    $("#upper-bound-thumbnail").html(` 
                <img class="thumbnail" height=50px src="${location.pathname + "/thumbnails/" + thumbnailMax}" loading="lazy">              
            `)


    displayThumbnails(page)
}

function displayThumbnails(page) {
    $("#thumbnails").empty();
    let shownThumbnailsData = [];
    for (let i = 0; i < Math.min(thumbnailsPerPage, shownThumbnails.length); i++) {
        const index = i + thumbnailsPerPage * page;
        const thumbnail = shownThumbnails[index];
        let nucleiCytoRatio = nucleiCytoRatios[sorted[shownThumbnailsIdx[index]]];
        nucleiCytoRatio = Math.round(nucleiCytoRatio * 10000) / 10000;
        if (!thumbnail) continue
        shownThumbnailsData.push({x: shownThumbnailsIdx[index], y: nucleiCytoRatio});
        $("#thumbnails").append(`
                    <table style="display: inline">
                        <tr>
                            <td>
                                <div style="text-align: center">
                                    <img class="thumbnail" src="${location.pathname + "/thumbnails/" + thumbnail}" loading="lazy">
                                </div>
                            </td>
                            
                        </tr>
                        <tr>
                            <td>
                                <p style="float: left">${nucleiCytoRatio}</p>
                                <button style="float: left" name="${index}" class="good_segmentation mdc-icon-button material-icons">thumb_up</button>
                                <button style="float: left" name="${index}" class="bad_segmentation mdc-icon-button material-icons">thumb_down</button>
                            </td>
                        </tr>
                    </table>
                    
                `);
    }
    $(".good_segmentation").click(function() {
        $(this).prop("disabled", true)
        showSnackbar(basicSnackbar, "Thanks for the feedback!")
    })
    $(".bad_segmentation").click(function() {
        $(this).prop("disabled", true)
        showSnackbar(basicSnackbar, "Thanks for the feedback!")
    })
    const shownLabel = "Shown Thumbnails";
    if (myChart.data.datasets[0].label === shownLabel) {
        myChart.data.datasets.shift();
    }
    myChart.data.datasets.splice(0, 0, {
        label: shownLabel,
        data: shownThumbnailsData,
        backgroundColor: 'rgb(0, 99, 132)'
    });
    myChart.update();
}


function filterData() {
    for (let i = 0; i < nucleiCytoRatios.length; i++) {
        const nucleiRatio = nucleiCytoRatios[i];
        if (nucleiRatio < preferences.minRatio || nucleiRatio > preferences.maxRatio) {
            nucleiCytoRatios.splice(i, 1);
            thumbnails.splice(i, 1);
            i--;
        }
    }
}
function parseData(exportData) {
    nucleiCytoRatios = exportData["nucleiCytoRatios"];
    thumbnails = exportData["thumbnails"];
    sorted = exportData["sorted"];

    filterData();

}

function showChart() {
    let ctx = $("#myChart");
    $("#lower-ratio").val("0");
    $("#lower-ratio").focus();
    $("#lower-ratio").blur();

    $("#upper-ratio").val(nucleiCytoRatios[sorted[thumbnails.length - 1]]);
    $("#upper-ratio").focus();
    $("#upper-ratio").blur();

    const chart_data = [];

    for (const i in thumbnails) {
        const nucleiRatio = nucleiCytoRatios[sorted[i]];
        chart_data.push({x: i, y: nucleiRatio});
    }

    myChart = new Chart(ctx, {
        type: 'scatter',
        data: {
            datasets: [{
                label: 'Cyto to Nuclei Ratios',
                data: chart_data,
                backgroundColor: 'rgb(255, 99, 132)'
            }],
        },
        options: {
            scales: {
                x: {
                    type: 'linear',
                    position: 'bottom'
                }
            }
        }
    });
}



