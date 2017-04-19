if [ -e ./qa_BasicTest_Data_hackrf.xml ] && [ -e ./qa_BasicTest_Data_rtlsdr.xml ] && [ -e ./qa_BasicTest_Data_usrp.xml ]; then
    mv ./qa_BasicTest_Data.xml ./qa_BasicTest_Data_extra.xml
    mv ./qa_BasicTest_Data_hackrf.xml ./qa_BasicTest_Data.xml
    echo "Current: HACKRF TESTS"
elif [ -e ./qa_BasicTest_Data_rtlsdr.xml ] && [ -e ./qa_BasicTest_Data_usrp.xml ] && [ -e ./qa_BasicTest_Data_extra.xml ]; then
    mv ./qa_BasicTest_Data.xml ./qa_BasicTest_Data_hackrf.xml
    mv ./qa_BasicTest_Data_rtlsdr.xml ./qa_BasicTest_Data.xml
    echo "Current: RTL-SDR TESTS"
elif [ -e ./qa_BasicTest_Data_hackrf.xml ] && [ -e ./qa_BasicTest_Data_usrp.xml ] && [ -e ./qa_BasicTest_Data_extra.xml ]; then
    mv ./qa_BasicTest_Data.xml ./qa_BasicTest_Data_rtlsdr.xml
    mv ./qa_BasicTest_Data_usrp.xml ./qa_BasicTest_Data.xml
    echo "Current: USRP TESTS"
elif [ -e ./qa_BasicTest_Data_hackrf.xml ] && [ -e ./qa_BasicTest_Data_rtlsdr.xml ] && [ -e ./qa_BasicTest_Data_extra.xml ]; then
    mv ./qa_BasicTest_Data.xml ./qa_BasicTest_Data_usrp.xml
    mv ./qa_BasicTest_Data_extra.xml ./qa_BasicTest_Data.xml
    echo "Current: EXTRA TESTS"
fi
