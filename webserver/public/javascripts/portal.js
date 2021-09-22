getRequest(location.pathname + "/export.json", function(xmlHttpRequest) {
    if (xmlHttpRequest.status === 200) {

        const export_data = JSON.parse(xmlHttpRequest.response);
        const nucleiCytoRatios = export_data["nucleiCytoRatios"];
        const thumbnails = export_data["thumbnails"];


        const sorted = [...Array(thumbnails.length).keys()];
        sorted.sort(function(first, second) {
            return nucleiCytoRatios[first] - nucleiCytoRatios[second];
        })

        const chart_data = [];
        const sortedNucleiCytoRatios = [...nucleiCytoRatios].sort();


        for (const i in thumbnails) {
            chart_data.push({x: i, y: nucleiCytoRatios[sorted[i]]});
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

            let thumbnailMin = thumbnails[sorted[sorted.length - 1]]
            let thumbnailMax = thumbnails[sorted[0]]
            $("#info").append(`
                <table>
                    <tr>
                        <td>
                            <img class="thumbnail" height=200px src="${location.pathname + "/thumbnails/" + thumbnailMin}" loading="lazy">
                        </td>
                        <td>
                            <img class="thumbnail" height=200px src="${location.pathname + "/thumbnails/" + thumbnailMax}" loading="lazy">
                        </td>
                    </tr>
                    <tr>
                        <td>
                            <p style="text-align: center">Biggest nuclei to cyto ratio</p>
                        </td>
                        <td>
                            <p style="text-align: center">Smallest nuclei to cyto ratio</p>
                        </td>
                    </tr>
                </table>
            `)
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
                const nucleiCytoRatio = Math.round(nucleiCytoRatios[sorted[shownThumbnailsIdx[index]]] * 10000) / 10000;
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
            if (myChart.data.datasets[0].label == shownLabel) {
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


    } else if (xmlHttpRequest.status == 404) {

    }
})



