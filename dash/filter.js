function createArchFilters(tableSelector, navSelector) {
    const archValues = [];
    $(tableSelector + ' tbody tr').each(function() {
        const archValue = ($(this).attr('data-arch') || '').trim();
        if (archValue && !archValues.includes(archValue))
            archValues.push(archValue);
    });

    archValues.sort();

    const nav = $(navSelector);
    nav.empty();
    nav.append("<button type='button' class='arch-filter active' data-arch='all'>All</button>");

    const multiSelectHintClass = 'arch-filter-hint';
    if (!nav.siblings('.' + multiSelectHintClass).length)
        nav.after("<sub class='" + multiSelectHintClass + "'>Ctrl+Click to multi-select</sub>");

    archValues.forEach(function(archValue) {
        const archButton = $("<button type='button' class='arch-filter'></button>");
        archButton.attr('data-arch', archValue);
        archButton.text(archValue);
        nav.append(archButton);
    });
}

function applyArchFilter(tableSelector, navSelector, archValue, useMultiSelect) {
    const nav = $(navSelector);
    const allButton = nav.find(".arch-filter[data-arch='all']");
    const archButtons = nav.find('.arch-filter').not("[data-arch='all']");

    let selectedArchValues = archButtons.filter('.active').map(function() {
        return ($(this).attr('data-arch') || '').trim();
    }).get();

    if (useMultiSelect) {
        if (archValue === 'all') {
            selectedArchValues = [];
        } else if (selectedArchValues.includes(archValue)) {
            selectedArchValues = selectedArchValues.filter(function(selectedArchValue) {
                return selectedArchValue !== archValue;
            });
        } else {
            selectedArchValues.push(archValue);
        }
    } else {
        selectedArchValues = archValue === 'all' ? [] : [archValue];
    }

    const selectedArchSet = new Set(selectedArchValues.filter(Boolean));

    $(tableSelector + ' tbody tr').each(function() {
        const rowArch = ($(this).attr('data-arch') || '').trim();
        const showRow = selectedArchSet.size === 0 || selectedArchSet.has(rowArch);
        $(this).toggle(showRow);
    });

    allButton.toggleClass('active', selectedArchSet.size === 0);
    archButtons.each(function() {
        const buttonArch = ($(this).attr('data-arch') || '').trim();
        $(this).toggleClass('active', selectedArchSet.has(buttonArch));
    });
}
