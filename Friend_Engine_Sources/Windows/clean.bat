echo Removing intermediate files and directories of FriendEngine
rmdir FriendEngine\release /s /q
rmdir FriendEngine\debug /s /q
rmdir FriendEngine\FriendEngine\Release /s /q
rmdir FriendEngine\FriendEngine\debug /s /q
del   FriendEngine\FriendEngine.sdf
del   FriendEngine\FriendEngine.v12.suo

echo Removing intermediate files and directories of libROI
rmdir libroi\release /s /q
rmdir libroi\debug /s /q
rmdir libROI\libROI\Release /s /q
rmdir libROI\libROI\debug /s /q
del   libroi\libroi.sdf
del   libroi\libroi.suo
del   libroi\libroi.v12.suo

echo Removing intermediate files and directories of libMotor
rmdir libmotor\release /s /q
rmdir libmotor\debug /s /q
rmdir libmotor\libmotor\Release /s /q
rmdir libmotor\libmotor\debug /s /q
del   libmotor\libmotor.sdf
del   libmotor\libmotor.suo
del   libmotor\libmotor.v12.suo

echo Removing intermediate files and directories of libEmotionRoi
rmdir libEmotionROI\release /s /q
rmdir libEmotionROI\debug /s /q
rmdir libEmotionROI\libEmotionROI\Release /s /q
rmdir libEmotionROI\libEmotionROI\debug /s /q
del   libEmotionROI\libEmotionROI.sdf
del   libEmotionROI\libEmotionROI.v12.suo

echo Removing intermediate files and directories of libConnectivity
rmdir libConnectivity\release /s /q
rmdir libConnectivity\debug /s /q
rmdir libConnectivity\libConnectivity\Release /s /q
rmdir libConnectivity\libConnectivity\debug /s /q
del   libConnectivity\libConnectivity.sdf
del   libConnectivity\libConnectivity.v12.suo

echo Removing intermediate files and directories of libBrainDecoding
rmdir libBrainDecoding\release /s /q
rmdir libBrainDecoding\debug /s /q
rmdir libBrainDecoding\libBrainDecoding\Release /s /q
rmdir libBrainDecoding\libBrainDecoding\debug /s /q
del   libBrainDecoding\libBrainDecoding.sdf
del   libBrainDecoding\libBrainDecoding.v12.suo

echo Removing intermediate files and directories of buildAll
rmdir buildAll\release /s /q
rmdir buildAll\debug /s /q
del   buildAll\buildAll.sdf

echo Removing executable files
del Application\*.exe
del Application\*.ilk
del Application\*.dll

echo Deleting the testdata intermediate files and directories
rmdir ..\Friend_Engine_Sources\TestData\SUBJ002\input /s /q
rmdir ..\Friend_Engine_Sources\TestData\SUBJ002\glm /s /q
rmdir ..\Friend_Engine_Sources\TestData\SUBJ002\log /s /q
rmdir ..\Friend_Engine_Sources\TestData\SUBJ002\svm /s /q
rmdir ..\Friend_Engine_Sources\TestData\SUBJ002\preproc /s /q
del ..\Friend_Engine_Sources\TestData\SUBJ002\MNI2RFI_RFI2.mat
del ..\Friend_Engine_Sources\TestData\SUBJ002\RFI2MNI_RFI2.mat
del ..\Friend_Engine_Sources\TestData\SUBJ002\RFI2MNI_RFI2.nii