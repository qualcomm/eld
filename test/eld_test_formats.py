import lit.formats
from thin_archive_test_format import ThinArchiveTestFormat

def get_test_format(config, execute_external):
    if config.eld_option_name == "thin_archives":
        return ThinArchiveTestFormat(execute_external)
    return lit.formats.ShTest(execute_external)