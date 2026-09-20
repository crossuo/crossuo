# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein
# for revision info

# Creates a C resources file from a single input file
function(create_resource input output)
    # Create empty output file
    file(WRITE ${output} "")

    # Get filename without path
    get_filename_component(filename ${input} NAME)

    # Replace filename spaces, dots & hyphens for C compatibility
    string(REGEX REPLACE "\\.| |-" "_" filename ${filename})

    # Read hex data from file
    file(READ ${input} filedata HEX)

    # Convert hex data for C compatibility
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})

    # Append data to output file
    file(APPEND ${output}
        "#pragma once\n"
        "const unsigned char ${filename}[] = {${filedata}};\n"
        "const unsigned ${filename}_size = sizeof(${filename});\n"
    )
endfunction()
