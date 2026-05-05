function normalizeBuildField(value) {
    if (value == null)
        return '';

    return String(value).trim();
}

function renderStatusCell(buildState) {
    const normalizedState = normalizeBuildField(buildState).toUpperCase();
    const backgroundColor = normalizedState === 'PASS' ? 'lightgreen' : 'red';

    return "<td style='display: inline-block; background-color: " + backgroundColor + "; border-radius: 15px'>" + normalizedState + "</td>";
}

function renderBuildTable(tableSelector, navSelector, buildStates) {
    const tableBody = $(tableSelector + ' tbody');
    tableBody.empty();

    (buildStates || []).slice().reverse().forEach(function(rowData) {
        const row = $('<tr></tr>');
        const runId = normalizeBuildField(rowData.run_id);
        const arch = normalizeBuildField(rowData.arch);
        const branch = normalizeBuildField(rowData.branch);

        row.append("<td>" + normalizeBuildField(rowData.date) + " - " + normalizeBuildField(rowData.time) + "</td>");
        row.append("<td>" + runId + "</td>");
        row.append("<td>" + arch + "</td>");
        row.append("<td>" + branch + "</td>");
        row.append(renderStatusCell(rowData.state));
        row.attr('data-arch', arch);

        tableBody.append(row);
    });

    createArchFilters(tableSelector, navSelector);

    $(navSelector).off('click.archFilter').on('click.archFilter', '.arch-filter', function(event) {
        applyArchFilter(tableSelector, navSelector, $(this).attr('data-arch'), event.ctrlKey);
    });

    tableBody.off('click.openRun').on('click.openRun', 'tr', function() {
        const runId = $(this).find('td:eq(1)').text().trim();
        if (!runId)
            return;

        const url = 'https://github.com/qualcomm/eld/actions/runs/' + runId;
        window.open(url, '_blank');
    });
}
