# Copy public headers specified in CrashReporter-iOS target
mkdir -p build_output/headers
cp Source/CrashReporter.h build_output/headers/
cp Source/PLCrashNamespace.h build_output/headers/
cp Source/PLCrashReporter.h build_output/headers/
cp Source/PLCrashReport.h build_output/headers/
cp Source/PLCrashReportApplicationInfo.h build_output/headers/
cp Source/PLCrashReportRegisterInfo.h build_output/headers/
cp Source/PLCrashReportBinaryImageInfo.h build_output/headers/
cp Source/PLCrashReportStackFrameInfo.h build_output/headers/
cp Source/PLCrashReportExceptionInfo.h build_output/headers/
cp Source/PLCrashReportThreadInfo.h build_output/headers/
cp Source/PLCrashReportSystemInfo.h build_output/headers/
cp Source/PLCrashAsyncSignalInfo.h build_output/headers/
cp Source/PLCrashReportSymbolInfo.h build_output/headers/
cp Source/PLCrashReportProcessInfo.h build_output/headers/
cp Source/PLCrashReportSignalInfo.h build_output/headers/
cp Source/PLCrashAsync.h build_output/headers/
cp Source/PLCrashFeatureConfig.h build_output/headers/
cp Source/PLCrashReporterConfig.h build_output/headers/
cp Source/PLCrashReportMachExceptionInfo.h build_output/headers/
cp Source/PLCrashMacros.h build_output/headers/
cp Source/PLCrashReportTextFormatter.h build_output/headers/
cp Source/PLCrashReportFormatter.h build_output/headers/
cp Source/PLCrashReportMachineInfo.h build_output/headers/
cp Source/PLCrashReportProcessorInfo.h build_output/headers/

xcodebuild archive  -project 'CrashReporter.xcodeproj' \
                    -scheme 'CrashReporter-iOS-Device' \
                    -configuration Release \
                    -destination 'generic/platform=iOS' \
                    -archivePath 'build_output/Release-iphoneos.xcarchive' \
                    SKIP_INSTALL=NO
                
xcodebuild archive  -project 'CrashReporter.xcodeproj' \
                    -scheme 'CrashReporter-iOS-Simulator' \
                    -configuration Release \
                    -destination 'generic/platform=iOS Simulator' \
                    -archivePath 'build_output/Release-iphonesimulator.xcarchive' \
                    SKIP_INSTALL=NO

xcodebuild  -create-xcframework \
            -library 'build_output/Release-iphoneos.xcarchive/Products/usr/local/lib/libCrashReporter-iphoneos.a' -headers 'build_output/headers/' \
            -library 'build_output/Release-iphonesimulator.xcarchive/Products/usr/local/lib/libCrashReporter-iphonesimulator.a' -headers 'build_output/headers/' \
            -output 'build_output/CrashReporter.xcframework'
