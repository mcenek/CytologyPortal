getRequest(location.pathname + "/export.json", function(xmlHttpRequest) {
    if (xmlHttpRequest.status === 200) {

        const export_data = JSON.parse(xmlHttpRequest.response);
        let nucleiCytoRatios = export_data["nucleiCytoRatios"];
        let thumbnails = export_data["thumbnails"];


        for (let i = 0; i < nucleiCytoRatios.length; i++) {
            const nucleiRatio = nucleiCytoRatios[i];

            if (nucleiRatio < preferences.minRatio || nucleiRatio > preferences.maxRatio) {
                nucleiCytoRatios.splice(i, 1);
                thumbnails.splice(i, 1);
                i--;
            }
        }



        const sorted = [...Array(thumbnails.length).keys()];
        sorted.sort(function(first, second) {
            return nucleiCytoRatios[first] - nucleiCytoRatios[second];
        })

        const chart_data = [];

        for (const i in thumbnails) {
            const nucleiRatio = nucleiCytoRatios[sorted[i]];
            chart_data.push({x: i, y: nucleiRatio});
        }

        let myChart;
        $(document).ready(function() {
            let ctx = $("#myChart");
            $("#lower-ratio").val("0");
            $("#lower-ratio").focus();
            $("#lower-ratio").blur();

            $("#upper-ratio").val(nucleiCytoRatios[sorted[thumbnails.length - 1]]);
            $("#upper-ratio").focus();
            $("#upper-ratio").blur();

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


       });

        let shownThumbnails;
        let shownThumbnailsIdx;

        let page = 0;


        const thumbnailsPerPage = 20;
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



        $("#show-thumbnails").click(function() {
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
        });


        $("#thumbnails-prev").click(function() {
            if (page <= 0) return;
            page--;
            displayThumbnails(page);
        });

        $("#thumbnails-next").click(function() {
            if (page + 1 >= shownThumbnails.length / thumbnailsPerPage) return;
            page++;
            displayThumbnails(page);
        });


        $("#content").show();


    } else if (xmlHttpRequest.status === 404) {

    }
})

$(document).ready(function() {
    getRequest(location.pathname + "/log.txt", function (xmlHttpRequest) {
        console.log(xmlHttpRequest.status);
        if (xmlHttpRequest.status === 200) {
            $("#info").append(xmlHttpRequest.responseText.trim())

        }
    });
});