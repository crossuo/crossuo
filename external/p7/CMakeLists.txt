set(P7_SRC
  Sources/ClBaical.h
  Sources/ClFile.h
  Sources/Client.h
  Sources/ClNull.h
  Sources/ClText.h
  Sources/ClTextConsole.h
  Sources/ClTextFile.h
  Sources/ClTextSink.h
  Sources/ClTextSyslog.h
  Sources/Common.h
  Sources/CommonClient.h
  Sources/PacketsPool.h
  Sources/resource.h
  Sources/Socket.h
  Sources/targetver.h
  Sources/Telemetry.h
  Sources/TPackets.h
  Sources/Trace.h
  Sources/ClBaical.cpp
  Sources/ClFile.cpp
  Sources/Client.cpp
  Sources/ClNull.cpp
  Sources/ClText.cpp
  Sources/CRC32.cpp
  Sources/Proxy.cpp
  Sources/Telemetry.cpp
  Sources/Trace.cpp
)

set(P7_HEADERS
  Headers/P7_Client.h
  Headers/P7_Cproxy.h
  Headers/P7_Extensions.h
  Headers/P7_Server.h
  Headers/P7_Telemetry.h
  Headers/P7_Trace.h
)

set(P7_HEADER_PATH Headers)

include_directories("${P7_HEADER_PATH}")
include_directories(Shared/)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  include_directories(Shared/Platforms/Windows_x86/)
else()
  include_directories(Shared/Platforms/Linux_x86/)
endif()

add_library(p7 STATIC ${P7_SRC})
