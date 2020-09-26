XCFRAMEWORK_OUTPUT_DIR="build_output"
HEADERS_DIR="${XCFRAMEWORK_OUTPUT_DIR}/headers"
PROJECT_PATH="CrashReporter.xcodeproj"
PRODUCT_NAME="CrashReporter"
DEVICE_SCHEME_NAME="CrashReporter-iOS-Device"
SIMULATOR_SCHEME_NAME="CrashReporter-iOS-Simulator"
CONFIGURATION="Release"
XCODEBUILD_DEVICE_DESTINATION="generic/platform=iOS"
XCODEBUILD_SIMULATOR_DESTINATION="generic/platform=iOS Simulator"

rm -rf "$XCFRAMEWORK_OUTPUT_DIR"
mkdir -p "$HEADERS_DIR"

# Copy public headers specified in CrashReporter-iOS target
cp Source/CrashReporter.h "${HEADERS_DIR}" || exit
cp Source/PLCrashNamespace.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReporter.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReport.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportApplicationInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportRegisterInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportBinaryImageInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportStackFrameInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportExceptionInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportThreadInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportSystemInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashAsyncSignalInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportSymbolInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportProcessInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportSignalInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashAsync.h "${HEADERS_DIR}" || exit
cp Source/PLCrashFeatureConfig.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReporterConfig.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportMachExceptionInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashMacros.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportTextFormatter.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportFormatter.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportMachineInfo.h "${HEADERS_DIR}" || exit
cp Source/PLCrashReportProcessorInfo.h "${HEADERS_DIR}" || exit

DEVICE_ARCHIVE_PATH="${XCFRAMEWORK_OUTPUT_DIR}/${CONFIGURATION}-iphoneos.xcarchive"
SIMULATOR_ARCHIVE_PATH="${XCFRAMEWORK_OUTPUT_DIR}/${CONFIGURATION}-iphonesimulator.xcarchive"

DEVICE_LIBRARY_PATH="${DEVICE_ARCHIVE_PATH}/Products/usr/local/lib/lib${PRODUCT_NAME}-iphoneos.a"
SIMULATOR_LIBRARY_PATH="${SIMULATOR_ARCHIVE_PATH}/Products/usr/local/lib/lib${PRODUCT_NAME}-iphonesimulator.a"

archive_xcode_project () {
    xcodebuild archive  -project "${PROJECT_PATH}" \
                        -scheme "$1" \
                        -configuration "${CONFIGURATION}" \
                        -destination "$2" \
                        -archivePath "$3" \
                        BUILD_LIBRARIES_FOR_DISTRIBUTION=YES \
                        SKIP_INSTALL=NO 2>&1 \
                        || { echo "Archiving project with destination \""$2"\" have been failed" ; exit ; }
}

archive_xcode_project "${DEVICE_SCHEME_NAME}" "${XCODEBUILD_DEVICE_DESTINATION}" "${DEVICE_ARCHIVE_PATH}"

archive_xcode_project "${SIMULATOR_SCHEME_NAME}" "${XCODEBUILD_SIMULATOR_DESTINATION}" "${SIMULATOR_ARCHIVE_PATH}"

xcodebuild  -create-xcframework \
            -library "${DEVICE_LIBRARY_PATH}" -headers ""${HEADERS_DIR}"" \
            -library "${SIMULATOR_LIBRARY_PATH}" -headers ""${HEADERS_DIR}"" \
            -output "${XCFRAMEWORK_OUTPUT_DIR}/${PRODUCT_NAME}.xcframework" 2>&1 \
            || { echo "Creating XCFramework have been failed" ; exit ; }
