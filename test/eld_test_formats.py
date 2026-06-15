import lit.formats

from thin_archive_test_format import ThinArchiveTestFormat


def get_test_format(config, base_format):
    if config.eld_option_name == "thin_archives":
        return ThinArchiveTestFormat(base_format=base_format)
    return base_format
