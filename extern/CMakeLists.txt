# Copyright (c) Darrell Wright
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/beached/daw_json_link
#

# This prevents tests running on dependencies
set(DAW_ENABLE_TESTING OFF)

include(FetchContent)

FetchContent_Declare(
        daw-header-libraries
        GIT_REPOSITORY https://github.com/beached/header_libraries.git
)

FetchContent_Declare(
        daw-synchro
        GIT_REPOSITORY https://github.com/beached/atomic_wait.git
)

FetchContent_Declare(
        daw-parallel
        GIT_REPOSITORY https://github.com/beached/parallel.git
        GIT_TAG main
)

FetchContent_MakeAvailable(
        daw-header-libraries
        daw-parallel
        daw-synchro
)

