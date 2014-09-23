#include "newimage/newimageall.h"
#include "fslfuncs.h"
#include "process.h"
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include "filefuncs.h"

using namespace std;
using namespace NEWIMAGE;

#ifdef WINDOWS
#include <direct.h>
#define snprintf sprintf_s
#define chdir _chdir
char *extension="";
#else
char *extension=".gz";

#endif
// set socket data for response
void FriendProcess::setSocketfd(int Sock)
{
    vdb.setSocketfd(Sock);
}

// config file functions
void FriendProcess::readConfigFile(char *configFile)
{
   vdb.readConfigFile(configFile);
}

void FriendProcess::readConfigBuffer(char *buffer, int size)
{
   vdb.readConfigBuffer(buffer, size);
}

void FriendProcess::readConfig(CSimpleIniA &ini)
{
   vdb.readConfig(ini);
}

void FriendProcess::saveConfigBuffer(char *buffer, int size, char *configFile)
{
   vdb.saveConfigBuffer(buffer, size, configFile);
}

// preprare files and run fsl_glm on processed volumes
void FriendProcess::glm()
{
   char prefix[BUFF_SIZE], Pref[BUFF_SIZE];
   char fsfFile[BUFF_SIZE];
   char auxString[BUFF_SIZE];
   stringstream CmdLn;

   // confirming that all variables are set
   if (!vdb.rPrepVars) prepRealtimeVars();

   fprintf(stderr, "Generating 4D File\n");
   vdb.getFinalVolumeFormat(prefix);
   vdb.getPreprocVolumePrefix(Pref);
   
   // generating 4D FRIEND pipeline volume for the run. Here we performs the final step, sliding window mean
   estimateActivation(1, vdb.interval.maxIndex(), vdb.slidingWindowSize, prefix, vdb.trainGLM4DFile);

   // concatenating the .par (movements parameters) files
   generateConfoundFile(Pref, 1, vdb.interval.maxIndex(), vdb.parFile);
   
   // concatenating the .rms files
   generateRmsFile(Pref, 1, vdb.interval.maxIndex(), vdb.rmsFile);

   // generating the rotation graph png
   fprintf(stderr, "Generating movement and rms graphics\n");
   CmdLn << "fsl_tsplot -i " << vdb.parFile << " -t \"MCFLIRT estimated rotations (radians)\" -u 1 --start=1 --finish=3 -a x,y,z -w 640 -h 144 -o " << vdb.logDir << "rot" << vdb.trainFeatureSuffix << ".png";

   fsl_tsplot((char *)CmdLn.str().c_str());

   // generating the translations graph png
   CmdLn.str("");
   CmdLn << "fsl_tsplot -i " << vdb.parFile << " -t \"MCFLIRT estimated translations (mm)\" -u 1 --start=4 --finish=6 -a x,y,z -w 640 -h 144 -o " << vdb.logDir << "trans" << vdb.trainFeatureSuffix << ".png";

   fsl_tsplot((char *)CmdLn.str().c_str());

   // generating the rms graph png
   CmdLn.str("");
   CmdLn << "fsl_tsplot -i " << vdb.rmsFile << " -t \"MCFLIRT estimated mean displacement (mm)\" -u 1 --start=1 --finish=1 -a absolute -w 640 -h 144 -o " << vdb.logDir << "rms" << vdb.trainFeatureSuffix << ".png";

   fsl_tsplot((char *)CmdLn.str().c_str());

   // copying the concatenated movements parameter file to log dir
   CmdLn.str("");
   CmdLn << "/bin/cp -p " << vdb.parFile << " " << vdb.logDir << "confounds" <<  vdb.trainFeatureSuffix << ".txt";
   system(CmdLn.str().c_str());

   
   // generating the design matrix file
   if (vdb.interval.conditionNames.size() > 0)
   {
      fprintf(stderr, "Generating conditions files.\n");
      vdb.interval.generateConditionsBoxCar(vdb.glmDir);
      fprintf(stderr, "Generating FSF file.\n");

      strcpy(vdb.interval.glmDir, vdb.glmDir);
      vdb.interval.generateFSFFile(vdb.fsfFile, vdb.runSize, vdb.includeMotionParameters, 0);

      fprintf(stderr, "Running feat_model.\n");
      CmdLn.str("");
      CmdLn << "feat_model " << vdb.glmDir << vdb.subject << vdb.trainFeatureSuffix;
      if (vdb.includeMotionParameters) CmdLn << " " << vdb.parFile;
      chdir(vdb.glmDir);
      feat_model((char *)CmdLn.str().c_str());

      remove(vdb.contrastFile);
      sprintf(auxString, "%s%s%s%s", vdb.glmDir, vdb.subject, vdb.trainFeatureSuffix, ".con");
      rename(auxString, vdb.contrastFile);

      remove(vdb.glmMatrixFile);
      sprintf(auxString, "%s%s%s%s", vdb.glmDir, vdb.subject, vdb.trainFeatureSuffix, ".mat");
      rename(auxString, vdb.glmMatrixFile);
   }

   sprintf(auxString, "%s%s", vdb.inputDir, "RFI_binmask.nii");

   fprintf(stderr, "Making binary mask.\n");
   CmdLn.str("");
   CmdLn << "fslmaths " << vdb.maskFile << " -bin " << auxString << " -odt char";
   fslmaths((char *)CmdLn.str().c_str());

   fprintf(stderr, "Running fsl_glm\n");
   CmdLn.str("");
   CmdLn << "fsl_glm -i " << vdb.trainGLM4DFile << " -d " << vdb.glmMatrixFile << " -c " << vdb.contrastFile << " -m " << auxString << " -o " << vdb.glmDir << "betas" << vdb.trainFeatureSuffix << " --out_t=" << vdb.glmTOutput << " --out_f=" << vdb.glmDir << "pvalues" << vdb.trainFeatureSuffix;
   fsl_glm((char *)CmdLn.str().c_str());
   
   vdb.rGLM=true;
}

// performs the feature selection step
void FriendProcess::featureSelection()
{
   stringstream CmdLn;
   if (!vdb.rPrepVars) prepRealtimeVars();
   // generate file with the maximum T for each voxel in the contrasts made
   CmdLn << "fslmaths " << vdb.glmTOutput << " -Tmax " << vdb.featuresAllTrainSuffix;
   fslmaths((char *)CmdLn.str().c_str());

   // generates the mni mask in subject space
   if (fileExists(vdb.mniMask) && fileExists(vdb.mniTemplate))
   {
      char name[BUFF_SIZE], prefix[30]="_RFI2";
      
      extractFileName(vdb.mniMask, name);
      for (int t=0;t<strlen(name);t++)
      if (name[t] == '.') name[t] = '_';
      
      sprintf(vdb.subjectSpaceMask, "%s%s%s.nii%s", vdb.inputDir, name, vdb.trainFeatureSuffix, extension);
      
      // brings the mni mask to subject space
      MniToSubject(vdb.rfiFile, vdb.mniMask, vdb.mniTemplate, vdb.subjectSpaceMask, prefix);
   }
   
   if (vdb.useWholeSubjectSpaceMask)
   { // just use all mni mask (in native space)
      char outputFile[BUFF_SIZE];
      sprintf(outputFile, "%s.nii%s", vdb.featuresTrainSuffix, extension);
      copyfile(vdb.subjectSpaceMask, outputFile);
   }
   else
   {
      if (vdb.byCutOff)
      {
         // do threshold
         CmdLn.str("");
         CmdLn << "fslmaths " << vdb.featuresAllTrainSuffix << " -thr " << vdb.tTestCutOff << " " << vdb.featuresTrainSuffix;
         fslmaths((char *)CmdLn.str().c_str());
      }
      else
      {
         /// select a percentage of higher voxels
         volume<float> features;
         string featuresFile = vdb.featuresAllSuffix;
         featuresFile += vdb.trainFeatureSuffix; 
         read_volume(features, featuresFile);
         float value=features.percentile((double)1.0-(double)(vdb.percentileHigherVoxels/100.0));

         // do threshold
         CmdLn.str("");
         CmdLn << "fslmaths " << vdb.featuresAllTrainSuffix << " -thr " << value << " " << vdb.featuresTrainSuffix;
         fslmaths((char *)CmdLn.str().c_str());
	  }
   }

   // create binary version
   CmdLn.str("");
   CmdLn << "fslmaths " << vdb.featuresTrainSuffix << " -bin " << vdb.featuresTrainSuffix << "_bin -odt char";
   fslmaths((char *)CmdLn.str().c_str());

   vdb.rFeatureSel = true;
}

// call plugin train function
void FriendProcess::train()
{
   if (!vdb.rPrepVars) prepRealtimeVars();
   if (pHandler.callTrainFunction(vdb))
      vdb.rTrain=true;
}

// call plugin test function
void FriendProcess::test(int index, float &classNum, float &projection)
{
   if (!vdb.rPrepVars) prepRealtimeVars();
   pHandler.callTestFunction(vdb, index, classNum, projection);
}

// calculates the mean volume of an interval
// just exec : fslmaths volume1 -add volume2 ... volumeN -div n
void FriendProcess::baselineCalculation(int intervalIndex, char *baseline)
{
   char Pref[BUFF_SIZE];
   char suf[20]= "_mc.nii";
   char format[50], number[50];
   
   // facilitates the filename generation. Generates the number string with the correct zeros
   sprintf(format, "%c0%dd", '%', vdb.numberWidth);
   char *prefix = extractFileName(vdb.rawVolumePrefix);
   snprintf(Pref, BUFF_SIZE-1, "%s%s", vdb.preprocDir, prefix);
   free(prefix);

   stringstream osc;
   osc  << "fslmaths ";

   // iterating to add the filenames to command
   for(int t=vdb.averageMeanOffset; t<= (vdb.interval.intervals[intervalIndex].end-vdb.interval.intervals[intervalIndex].start); t++)
   {
      sprintf(number, format,(vdb.interval.intervals[intervalIndex].start + t));
      // if the second file, adds the `-add` token
      if (t > vdb.averageMeanOffset) osc << " -add ";
      // adds the file in command line
      osc << " " << Pref << number << suf;
   };

   // generates the -div N part
   sprintf(number, "%d",(vdb.interval.intervals[intervalIndex].end-vdb.interval.intervals[intervalIndex].start+1-vdb.averageMeanOffset));
   osc << " -div " << number << " " << baseline << '\0';

   // runs fslmaths
   fslmaths((char *)osc.str().c_str());
};

// verifies if the condition is a baseline condition
BOOL FriendProcess::isBaselineCondition(char * condition)
{
   return vdb.interval.isBaselineCondition(condition);
}

// verifies if the next file is ready to be read by FRIEND and transforms it in nifti accordingly
BOOL FriendProcess::isReadyNextFile(int index, char *rtPrefix, char *format, char *inFile)
{
   BOOL response=0;
   char  number[50], outFile[BUFF_SIZE], volumeName[BUFF_SIZE];
   sprintf(number, format, index);
   sprintf(inFile, "%s%s%s", vdb.rawVolumePrefix, number, ".img");
   sprintf(outFile, "%s%s%s", rtPrefix, number, ".nii");
   if (fileExists(inFile))
   {
	   if (isReadable(inFile))
	   {
		   stringstream osc;
		   osc << "fslswapdim " << inFile;

		   if (!vdb.invX) osc << " x";
		   else osc << " -x";

		   if (!vdb.invY) osc << " y";
		   else osc << " -y";

		   if (!vdb.invZ) osc << " z";
		   else osc << " -z";

		   osc << "  " << outFile << '\0';
		   // transforms an analyze file into a nii file, inverting axes accordingly
		   //fprintf(stderr, "%s", osc.str().c_str());
		   fslSwapDimRT(osc.str().c_str(), vdb.runReferencePtr);

		   osc.str("");
		   osc << "fslmaths " << outFile << " " << outFile << " -odt float";
		   fslmaths((char *)osc.str().c_str());


		   if ((fileExists(outFile)) && (isReadable(outFile))) 
			   response = 1;
	   }
   }
   else 
   { // verifying if the final file exists
	   if ((fileExists(outFile)) && (isReadable(outFile)))
       {
         response = 1;
       }
       else response = 0;
   }
   
   if (response)
   {
      if (returnFileNameExists(outFile, volumeName))
         pHandler.callVolumeFunction(vdb, index, volumeName);
   }
      
   return response;
}

// generate a file concatenating all movement parameters of the processed volumes
void FriendProcess::generateConfoundFile(char *dPrefix, int ini, int end, char *output)
{
   char format[50], number[50], parFileName[BUFF_SIZE];
   sprintf(format, "%c0%dd", '%', vdb.numberWidth);
   fstream Output(output, fstream::in | fstream::out | fstream::trunc);
   for (int t=ini; t<=end;t++)
   {
       sprintf(number, format, t);
       sprintf(parFileName, "%s%s%s", dPrefix, number, "_mc.nii.par");
       fstream parFile(parFileName, fstream::in);
       
       float rx, ry, rz, tx, ty, tz;
       parFile >> rx >> ry >> rz >> tx >> ty >> tz;
       Output << rx << " " << ry << " " << rz <<  " " << tx << " " << ty << " " << tz << '\n';
   }
   Output.close();
}

// generate a file concatenating all the processed volumes rms values
void FriendProcess::generateRmsFile(char *dPrefix, int ini, int end, char *output)
{
   char format[50], number[50], rmsFilename[BUFF_SIZE];
   sprintf(format, "%c0%dd", '%', vdb.numberWidth);
   fstream Output(output, fstream::in | fstream::out | fstream::trunc);
   for (int t=ini; t<=end;t++)
   {
       sprintf(number, format, t);
       sprintf(rmsFilename, "%s%s%s", dPrefix, number, "_mc.nii_abs.rms");
       fstream rmsFile(rmsFilename, fstream::in);
       
       float rms;
       rmsFile >> rms;
       Output << rms << '\n';
   }
   Output.close();
}

// runs the real time pipeline at once. Calls the realtimePipelineStep for each volume
void FriendProcess::runRealtimePipeline()
{
    char preprocVolumePrefix[BUFF_SIZE];
    char format[30];
    char msg[50];
   
	fprintf(stderr, "processing pipeline begin\n");
    if (!vdb.rPrepVars) prepRealtimeVars();
    vdb.actualBaseline[0]=0;
    vdb.actualImg = 1;
    vdb.actualInterval = 0;
    vdb.getFormat(format);
    
    removeDirectory(vdb.preprocDir);
#ifdef WIN32
    _mkdir(vdb.preprocDir);
#else
    mkdir(vdb.preprocDir, 0777); // notice that 777 is different than 0777
#endif

    // getting the preproc volume prefix
    vdb.getPreprocVolumePrefix(preprocVolumePrefix);
   
    // setting the reference volume
    sprintf(vdb.motionRefVolume, "%s", vdb.rfiFile);
   
    // getting the FSLIO pointer of the reference volume for fslwapdimRT calls
    vdb.runReferencePtr = fslioopen(vdb.motionRefVolume);
   
    // reading the design file
    vdb.interval.readDesignFile(vdb.designFile);
   
    // looping
    while (vdb.actualImg <= vdb.runSize)
    {
        realtimePipelineStep(preprocVolumePrefix, format, vdb.actualBaseline);
    }
    fslioclose(vdb.runReferencePtr);
   
    // issuing the end of run response
    if (vdb.sessionPointer == NULL)
    {
       sprintf(msg, "%s", "ENDGRAPH\n");
       vdb.socks.writeString(msg);
       vdb.rPipeline = true;
    }
	fprintf(stderr, "processing pipeline end\n");
}


// send Graph params to FRONTEND process. Maybe latter changing to JSON
void FriendProcess::sendGraphParams(char *mcfile, char *number)
{
    fstream gfile;
   
    char parFile[BUFF_SIZE], rmsFile[BUFF_SIZE];
    sprintf(parFile,  "%s%s", mcfile, ".par");
    sprintf(rmsFile,  "%s%s", mcfile, "_abs.rms");
   
    float value;
    stringstream msg;
   
    gfile.open(parFile, fstream::in | fstream::out);
    msg << "GRAPHPARS;" << number << ";";
    gfile >> value;
    msg << value << ";";
   
    gfile >> value;
    msg << value << ";";
   
    gfile >> value;
    msg << value << ";";
   
     gfile >> value;
    msg << value << ";";
   
    gfile >> value;
    msg << value << ";";
   
    gfile >> value;
    msg << value << ";";
   
    gfile.close();
    gfile.open(rmsFile, fstream::in | fstream::out);
   
    gfile >> value;
    msg << value << '\n';
    gfile.close();
   
    if (vdb.sessionPointer == NULL)
    {
       fprintf(stderr, "sending : %s\n", msg.str().c_str());
       vdb.socks.writeString(msg.str().c_str());
    }
    else
    {
       vdb.sessionPointer->processGraphMessage(msg.str().c_str());
    }
}

// theoretically loading the default plugin library and functions
void FriendProcess::loadLibrary()
{
}

// unloading the plugin library
void FriendProcess::unLoadLibrary()
{
   pHandler.unLoadLibrary();
}

// set the library file and function names of the plugIn
void FriendProcess::loadFunctions(char *library, char *trainFunc, char *testFunc, char *initFunc, char *finalFunc, char *volumeFunc, char *afterPreProcFunc)
{
   pHandler.loadFunctions(library, trainFunc, testFunc, initFunc, finalFunc, volumeFunc, afterPreProcFunc);
   if (vdb.rPrepVars) pHandler.callInitFunction(vdb);
}

// set the path for plug in library file
void FriendProcess::setLibraryPath(char *path)
{
   pHandler.setLibraryPath(path);
}

// process one volume in the pipeline
void FriendProcess::realtimePipelineStep(char *rtPrefix, char *format, char *actualBaseline)
{
   char mcfile[BUFF_SIZE], mcgfile[BUFF_SIZE], inFile[BUFF_SIZE], outFile[BUFF_SIZE], CmdLn[BUFF_SIZE], matOldName[BUFF_SIZE], matNewName[BUFF_SIZE], number[50];
   if (isReadyNextFile(vdb.actualImg, rtPrefix, format, inFile))
   {
      fprintf(stderr, "Processing file = %s\n", inFile);
      sprintf(number, format, vdb.actualImg);
      sprintf(inFile, "%s%s", rtPrefix, number);
      vdb.getMCVolumeName(mcfile, number);
      vdb.getMCGVolumeName(mcgfile, number);
      vdb.getFinalVolumeName(outFile, number);
      sprintf(CmdLn, "mcflirt -in %s -reffile %s -out %s %s", inFile, vdb.motionRefVolume, mcfile, vdb.mcflirtParams);


      // mcFlirt
      mcflirt(CmdLn);
      
      // copying the .mat file
      sprintf(matOldName, "%s%s%c%s", mcfile, ".mat", PATHSEPCHAR, "MAT_0000");
      sprintf(matNewName,  "%s%s%s", rtPrefix, number, "_mc.mat");
      rename(matOldName, matNewName);
      sprintf(matOldName, "%s%s", mcfile, ".mat");
      rmdir(matOldName);

      // subtraction process
      // ending of the block ?
      if (vdb.actualImg > vdb.interval.intervals[vdb.actualInterval].end)
      {
         // Baseline Calculation, if this is a baseline block
         if (isBaselineCondition(vdb.interval.intervals[vdb.actualInterval].condition)) 
         {
            fprintf(stderr, "Baseline mean calculation\n");
            sprintf(actualBaseline, "%s%s%d%s", vdb.preprocDir, "mc_bsl", (vdb.actualInterval+1), "mean.nii");
            baselineCalculation(vdb.actualInterval, vdb.actualBaseline);
         }
         // going to the next interval
         vdb.actualInterval++;
      };

      // sctual subtraction
      if (vdb.actualBaseline[0] != 0)
      {
         sprintf(CmdLn, "fslmaths %s -sub %s %s", mcfile, vdb.actualBaseline, outFile);
         fslmaths(CmdLn);
      }
      else // if no baseline mean already calculated, zeroes volume. Note zeroing the supposed subtracted volume
      {
         sprintf(CmdLn, "fslmaths %s -mul 0 %s", mcfile, outFile);
         fslmaths(CmdLn);
      }

      // gaussian filtering the subtracted volume
      sprintf(CmdLn, "fslmaths %s -kernel gauss %f -fmean %s", outFile, (float) vdb.FWHM/2.3548, outFile);
      fslmaths(CmdLn);

      // gaussian filtering the motion corrected volume
      sprintf(CmdLn, "fslmaths %s -kernel gauss %f -fmean %s", mcfile, (float) vdb.FWHM/2.3548, mcgfile);
      fslmaths(CmdLn);

      // send graph params to FRONT END
      sendGraphParams(mcfile, number);
      pHandler.callAfterPreprocessingFunction(vdb, vdb.actualImg, outFile);
      
      if (vdb.sessionPointer != NULL)
         if (vdb.sessionPointer->getFeedbackResponses)
         {
            float classNum, feedBackResponse;
            test(vdb.actualImg, classNum, feedBackResponse);
            vdb.sessionPointer->processFeedback(vdb.actualImg, classNum, feedBackResponse);
         }
      vdb.actualImg++;
   }
}

// initializating control variables
void FriendProcess::initializeStates()
{
   vdb.initializeStates();
}

// initializating all other variables
void FriendProcess::prepRealtimeVars()
{
   vdb.prepRealtimeVars();
   fprintf(stderr, "Will call init function.\n");
   pHandler.callInitFunction(vdb);
   vdb.rPrepVars=1;
}

// change a config file variable value
void FriendProcess::setVar(char *var, char *value)
{
   vdb.setVar(var, value);
}

// clean up memory the code before exit
void FriendProcess::cleanUp()
{

}

// make the last steps after the processing of the run
void FriendProcess::wrapUpRun()
{
   char newpreprocDir[BUFF_SIZE], tempVolume[BUFF_SIZE];
   
   // deleting the temporary volume
   sprintf(tempVolume, "%s%s",  vdb.outputDir, "temp.nii");
   remove(tempVolume);
   
   // renaming the preproc directory
   sprintf(newpreprocDir, "%s%s%s",  vdb.outputDir, "preproc", vdb.trainFeatureSuffix);
   if (vdb.rPipeline)
   {
      if (fileExists(newpreprocDir)) removeDirectory(newpreprocDir);
      rename(vdb.preprocDir, newpreprocDir);
   }
   
}

void FriendProcess::setSessionPointer(Session *sessionPtr)
{
   vdb.sessionPointer = sessionPtr;
   if (sessionPtr != NULL)
      sessionPtr->setVDBPointer(&vdb);
}

int FriendProcess::isConfigRead()
{
	return vdb.rIniRead;
}

// automatic feedback calculations
void FriendProcess::setFeedbackCalculation(int automaticCalculations)
{
   if (vdb.sessionPointer != NULL)
      vdb.sessionPointer->getFeedbackResponses = automaticCalculations;
}

// sets the status phase 0 : begin 1 : end
void FriendProcess::setPhaseStatus(string phase, int status)
{
   if (vdb.sessionPointer != NULL)
      vdb.sessionPointer->setCommandResponse(phase, status);
}

// gets the status phase
void FriendProcess::getPhaseStatus(string phase, char *response)
{
   if (vdb.sessionPointer != NULL)
      vdb.sessionPointer->getCommandResponse(phase, response);
}

// steps before realtime processing
void FriendProcess::prepRealTime()
{
   char arqAxial[BUFF_SIZE] = { }, CmdLn[BUFF_SIZE] = { }, betAnat[BUFF_SIZE] = { };
   size_t buffSize = BUFF_SIZE-1;
   
   if (!vdb.rPrepVars)
   {
	   fprintf(stderr, "Calling PrepRealtime vars.\n");
	   prepRealtimeVars();
   }
	  

   if (!fileExists(vdb.baseImage))
   {
      // Anatomic Processing
      snprintf(arqAxial, buffSize, "%s%s", vdb.inputDir, "RAI_ax.nii");
      resampleVolume(vdb.raiFile, arqAxial, 1, 1, 1, vdb.TR, 0);
	  axial(arqAxial, arqAxial);
	  centralizeVolume(arqAxial, vdb.baseImage);
   }

   // creating a resampled anatomic with the same voxel dim as the functional, for registering the pipelines outcomes with this volume for presenting in FRONT END. Actually the FRIEND engine does not do this corregistering
   if (!fileExists(vdb.baseFunctional))
      equalVoxelDim(vdb.baseImage, vdb.rfiFile, vdb.baseFunctional, vdb.TR, 0);

   // bet Functional
   if (!fileExists(vdb.maskFile))
   {
      snprintf(CmdLn, buffSize, "bet %s %s", vdb.rfiFile, vdb.maskFile);
      bet(CmdLn);

      snprintf(CmdLn, buffSize, "bet %s %s %s", vdb.maskFile, vdb.maskFile, vdb.betParameters);
      bet(CmdLn);
   }
 
   // Coregistering RFI to RAI
   if (!fileExists(vdb.matrixFile))
   {
      snprintf(betAnat, buffSize, "%s%s", vdb.inputDir, "RAI_ax_cube_sks.nii");

      snprintf(CmdLn, buffSize, "bet %s %s", vdb.baseImage, betAnat);
      bet(CmdLn);

      snprintf(CmdLn, buffSize, "bet %s %s", betAnat, betAnat);
      bet(CmdLn);

      snprintf(CmdLn, buffSize, "flirt -ref %s -in %s -dof 7 -omat %s", betAnat, vdb.maskFile, vdb.matrixFile);
      flirt(CmdLn);
   }
   vdb.rPreProc=true;
}
