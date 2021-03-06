/*  cluster.cc

    Mark Jenkinson & Matthew Webster, FMRIB Image Analysis Group

    Copyright (C) 2000-2008 University of Oxford  */

/*  Part of FSL - FMRIB's Software Library
    http://www.fmrib.ox.ac.uk/fsl
    fsl@fmrib.ox.ac.uk
    
    Developed at FMRIB (Oxford Centre for Functional Magnetic Resonance
    Imaging of the Brain), Department of Clinical Neurology, Oxford
    University, Oxford, UK
    
    
    LICENCE
    
    FMRIB Software Library, Release 4.0 (c) 2007, The University of
    Oxford (the "Software")
    
    The Software remains the property of the University of Oxford ("the
    University").
    
    The Software is distributed "AS IS" under this Licence solely for
    non-commercial use in the hope that it will be useful, but in order
    that the University as a charitable foundation protects its assets for
    the benefit of its educational and research purposes, the University
    makes clear that no condition is made or to be implied, nor is any
    warranty given or to be implied, as to the accuracy of the Software,
    or that it will be suitable for any particular purpose or for use
    under any specific conditions. Furthermore, the University disclaims
    all responsibility for the use which is made of the Software. It
    further disclaims any liability for the outcomes arising from using
    the Software.
    
    The Licensee agrees to indemnify the University and hold the
    University harmless from and against any and all claims, damages and
    liabilities asserted by third parties (including claims for
    negligence) which arise directly or indirectly from the use of the
    Software or the sale of any products based on the Software.
    
    No part of the Software may be reproduced, modified, transmitted or
    transferred in any form or by any means, electronic or mechanical,
    without the express permission of the University. The permission of
    the University is not required if the said reproduction, modification,
    transmission or transference is done without financial return, the
    conditions of this Licence are imposed upon the receiver of the
    product, and all original and amended source code is included in any
    transmitted product. You may be held legally responsible for any
    copyright infringement that is caused or encouraged by your failure to
    abide by these terms and conditions.
    
    You are not permitted under this Licence to use this Software
    commercially. Use for which any financial return is received shall be
    defined as commercial use, and includes (1) integration of all or part
    of the source code or the Software into a product for sale or license
    by or on behalf of Licensee to third parties or (2) use of the
    Software or any derivative of it for research with the final aim of
    developing software products for sale or license to a third party or
    (3) use of the Software or any derivative of it for research with the
    final aim of developing non-software products for sale or license to a
    third party, or (4) use of the Software to provide any service to an
    external organisation for which payment is received. If you are
    interested in using the Software commercially, please contact Isis
    Innovation Limited ("Isis"), the technology transfer company of the
    University, to negotiate a licence. Contact details are:
    innovation@isis.ox.ac.uk quoting reference DE/1112. */

#ifndef EXPOSE_TREACHEROUS
#define EXPOSE_TREACHEROUS
#endif    

#include <vector>
#include <algorithm>
#include <iomanip>
#include "newimage/newimageall.h"
#include "utils/options.h"
#include "infer.h"
#include "warpfns/warpfns.h"
#include "warpfns/fnirt_file_reader.h"
#include "parser.h"

#define _GNU_SOURCE 1
#define POSIX_SOURCE 1        

using namespace NEWIMAGE;
using std::vector;
using namespace Utilities;
using namespace NEWMAT;
using namespace MISCMATHS;

namespace cluster {
#include "newimage/fmribmain.h"
string title="cluster (Version 1.3)\nCopyright(c) 2000-2008, University of Oxford (Mark Jenkinson, Matthew Webster)";
string examples="cluster --in=<filename> --thresh=<value> [options]";

Option<bool> verbose(string("-v,--verbose"), false, 
		     string("switch on diagnostic messages"), 
		     false, no_argument);
Option<bool> help(string("-h,--help"), false,
		  string("display this message"),
		  false, no_argument);
Option<bool> mm(string("--mm"), false,
		  string("use mm, not voxel, coordinates"),
		  false, no_argument);
Option<bool> minv(string("--min"), false,
		  string("find minima instead of maxima"),
		  false, no_argument);
Option<bool> fractional(string("--fractional"), false,
			string("interprets the threshold as a fraction of the robust range"),
			false, no_argument);
Option<bool> no_table(string("--no_table"), false,
		      string("suppresses printing of the table info"),
		      false, no_argument);
Option<bool> minclustersize(string("--minclustersize"), false,
		      string("prints out minimum significant cluster size"),
		      false, no_argument);
Option<int> voxvol(string("--volume"), 0,
		   string("number of voxels in the mask"),
		   false, requires_argument);
Option<int> numconnected(string("--connectivity"), 26,
		   string("the connectivity of voxels (default 26)"),
		   false, requires_argument);
Option<int> mx_cnt(string("-n,--num"), 6,
		   string("no of local maxima to report"),
		   false, requires_argument);
Option<float> dLh(string("-d,--dlh"), 4.0,
		  string("smoothness estimate = sqrt(det(Lambda))"),
		  false, requires_argument);
Option<float> thresh(string("-t,--thresh,--zthresh"), 2.3,
		     string("threshold for input volume"),
		     true, requires_argument);
Option<float> pthresh(string("-p,--pthresh"), 0.01,
		      string("p-threshold for clusters"),
		      false, requires_argument);
Option<float> peakdist(string("--peakdist"), 0,
		      string("minimum distance between local maxima/minima, in mm (default 0)"),
		      false, requires_argument);
Option<string> inputname(string("-i,--in,-z,--zstat"), string(""),
			 string("filename of input volume"),
			 true, requires_argument);
Option<string> copename(string("-c,--cope"), string(""),
			string("filename of input cope volume"),
			false, requires_argument);
Option<string> outpvals(string("--opvals"), string(""),
			string("filename for image output of log pvals"),
			false, requires_argument);
Option<string> outindex(string("-o,--oindex"), string(""),
			string("filename for output of cluster index (in size order)"),
			false, requires_argument);
Option<string> outlmax(string("--olmax"), string(""),
			string("filename for output of local maxima text file"),
			false, requires_argument);
Option<string> outlmaxim(string("--olmaxim"), string(""),
			string("filename for output of local maxima volume"),
			false, requires_argument);
Option<string> outthresh(string("--othresh"), string(""),
			 string("filename for output of thresholded image"),
			 false, requires_argument);
Option<string> outsize(string("--osize"), string(""),
		       string("filename for output of size image"),
		       false, requires_argument);
Option<string> outmax(string("--omax"), string(""),
		      string("filename for output of max image"),
		      false, requires_argument);
Option<string> outmean(string("--omean"), string(""),
		       string("filename for output of mean image"),
		       false, requires_argument);
Option<string> transformname(string("-x,--xfm"), string(""),
		       string("filename for Linear: input->standard-space transform. Non-linear: input->highres transform"),
		       false, requires_argument);
Option<string> stdvolname(string("--stdvol"), string(""),
		       string("filename for standard-space volume"),
		       false, requires_argument);
Option<string> warpname(string("--warpvol"), string(""),
		       string("filename for warpfield"),
		       false, requires_argument);


int num(const char x) { return (int) x; }
short int num(const short int x) { return x; }
int num(const int x) { return x; }
float num(const float x) { return x; }
double num(const double x) { return x; }


template <class T>
struct triple { T x; T y; T z; };

template <class T, class S>
void copyconvert(const vector<triple<T> >& oldcoords, 
		 vector<triple<S> >& newcoords)
{
  newcoords.erase(newcoords.begin(),newcoords.end());
  newcoords.resize(oldcoords.size());
  for (unsigned int n=0; n<oldcoords.size(); n++) {
    newcoords[n].x = (S) oldcoords[n].x;
    newcoords[n].y = (S) oldcoords[n].y;
    newcoords[n].z = (S) oldcoords[n].z;
  }
}

template <class T>
void MultiplyCoordinateVector(vector<triple<T> >& coords, const Matrix& mat)
{
  ColumnVector vec(4);
  for (unsigned int n=0; n<coords.size(); n++) {
    vec << coords[n].x << coords[n].y << coords[n].z << 1.0;
    vec = mat * vec;     // apply voxel xfm
    coords[n].x = vec(1);
    coords[n].y = vec(2);
    coords[n].z = vec(3);
  }
}

template <class T, class S>
void TransformToReference(vector<triple<T> >& coordlist, const Matrix& affine, 
			  const volume<S>& source, const volume<S>& dest, const volume4D<float>& warp,bool doAffineTransform, bool doWarpfieldTransform)
{
  ColumnVector coord(4);
  for (unsigned int n=0; n<coordlist.size(); n++) {
    coord << coordlist[n].x << coordlist[n].y << coordlist[n].z << 1.0;
    if ( doAffineTransform && doWarpfieldTransform ) coord = NewimageCoord2NewimageCoord(affine,warp,true,source,dest,coord);
    if ( doAffineTransform && !doWarpfieldTransform) coord = NewimageCoord2NewimageCoord(affine,source,dest,coord);
    if ( !doAffineTransform && doWarpfieldTransform) coord = NewimageCoord2NewimageCoord(warp,true,source,dest,coord);
    coordlist[n].x = coord(1);
    coordlist[n].y = coord(2);
    coordlist[n].z = coord(3);
  }
}

template <class T>
bool checkIfLocalMaxima(const int& index, const volume<int>& labelim, const volume<T>& zvol, const int& connectivity, const int& x, const int& y, const int& z )
{	       
  if (connectivity==6)
    return ( index==labelim(x,y,z) &&
	     zvol(x,y,z)>zvol(x,  y,  z-1) &&
	     zvol(x,y,z)>zvol(x,  y-1,z) &&
	     zvol(x,y,z)>zvol(x-1,y,  z) &&
	     zvol(x,y,z)>=zvol(x+1,y,  z) &&
	     zvol(x,y,z)>=zvol(x,  y+1,z) &&
	     zvol(x,y,z)>=zvol(x,  y,  z+1) );

  else 
    return ( index==labelim(x,y,z) &&
	     zvol(x,y,z)>zvol(x-1,y-1,z-1) &&
	     zvol(x,y,z)>zvol(x,  y-1,z-1) &&
	     zvol(x,y,z)>zvol(x+1,y-1,z-1) &&
	     zvol(x,y,z)>zvol(x-1,y,  z-1) &&
	     zvol(x,y,z)>zvol(x,  y,  z-1) &&
	     zvol(x,y,z)>zvol(x+1,y,  z-1) &&
	     zvol(x,y,z)>zvol(x-1,y+1,z-1) &&
	     zvol(x,y,z)>zvol(x,  y+1,z-1) &&
	     zvol(x,y,z)>zvol(x+1,y+1,z-1) &&
	     zvol(x,y,z)>zvol(x-1,y-1,z) &&
	     zvol(x,y,z)>zvol(x,  y-1,z) &&
	     zvol(x,y,z)>zvol(x+1,y-1,z) &&
	     zvol(x,y,z)>zvol(x-1,y,  z) &&
	     zvol(x,y,z)>=zvol(x+1,y,  z) &&
	     zvol(x,y,z)>=zvol(x-1,y+1,z) &&
	     zvol(x,y,z)>=zvol(x,  y+1,z) &&
	     zvol(x,y,z)>=zvol(x+1,y+1,z) &&
	     zvol(x,y,z)>=zvol(x-1,y-1,z+1) &&
	     zvol(x,y,z)>=zvol(x,  y-1,z+1) &&
	     zvol(x,y,z)>=zvol(x+1,y-1,z+1) &&
	     zvol(x,y,z)>=zvol(x-1,y,  z+1) &&
	     zvol(x,y,z)>=zvol(x,  y,  z+1) &&
	     zvol(x,y,z)>=zvol(x+1,y,  z+1) &&
	     zvol(x,y,z)>=zvol(x-1,y+1,z+1) &&
	     zvol(x,y,z)>=zvol(x,  y+1,z+1) &&
	     zvol(x,y,z)>=zvol(x+1,y+1,z+1) );

}

template <class T>
void get_stats(const volume<int>& labelim, const volume<T>& origim,
	       vector<int>& size,
	       vector<T>& maxvals, vector<float>& meanvals,
	       vector<triple<int> >& max, vector<triple<float> >& cog,
	       bool minv) 
{
  int labelnum = labelim.max();
  size.resize(labelnum+1,0);
  maxvals.resize(labelnum+1, (T) 0);
  meanvals.resize(labelnum+1,0.0f);
  triple<int> zero;
  zero.x = 0; zero.y = 0; zero.z = 0;
  triple<float> zerof;
  zerof.x = 0; zerof.y = 0; zerof.z = 0;
  max.resize(labelnum+1,zero);
  cog.resize(labelnum+1,zerof);
  vector<float> sum(labelnum+1,0.0);
  for (int z=labelim.minz(); z<=labelim.maxz(); z++) {
    for (int y=labelim.miny(); y<=labelim.maxy(); y++) {
      for (int x=labelim.minx(); x<=labelim.maxx(); x++) {
	int idx = labelim(x,y,z);
	T oxyz = origim(x,y,z);
	size[idx]++;
	cog[idx].x+=((float) oxyz)*x;
	cog[idx].y+=((float) oxyz)*y;
	cog[idx].z+=((float) oxyz)*z;
	sum[idx]+=(float) oxyz;
	if ( (size[idx]==1) || 
	     ( (oxyz>maxvals[idx]) && (!minv) ) || 
	     ( (oxyz<maxvals[idx]) && (minv) ) ) 
	  {
	    maxvals[idx] = oxyz;
	    max[idx].x = x;
	    max[idx].y = y;
	    max[idx].z = z;
	  }
      }
    }
  }
  for (int n=0; n<=labelnum; n++) {
    if (size[n]>0.0) {
      meanvals[n] = (sum[n]/((float) size[n]));
    }
    if (sum[n]>0.0) {
      cog[n].x /= sum[n];
      cog[n].y /= sum[n];
      cog[n].z /= sum[n];
    }
  }
}


vector<int> get_sortindex(const vector<int>& vals)
{
  // return the mapping of old indices to new indices in the
  //   new *ascending* sort of vals
  int length=vals.size();
  vector<pair<int, int> > sortlist(length);
  for (int n=0; n<length; n++) {
    sortlist[n] = pair<int, int>(vals[n],n);
  }
  sort(sortlist.begin(),sortlist.end());  // O(N.log(N))
  vector<int> idx(length);
  for (int n=0; n<length; n++) {
    idx[n] = sortlist[n].second;
  }
  return idx;
}


void get_sizeorder(const vector<int>& size, vector<int>& sizeorder) 
{
  vector<int> sizecopy(size), idx;
  idx = get_sortindex(sizecopy);

  // second part of pair is now the prior-index of the sorted values
  int length = size.size();
  sizeorder.resize(length,0);
  for (int n=0; n<length; n++) {
    sizeorder[idx[n]] = n;    // maps old index to new
  }
}


template <class T, class S>
void relabel_image(const volume<int>& labelim, volume<T>& relabelim,
		   const vector<S>& newlabels)
{
  copyconvert(labelim,relabelim);
  for (int z=relabelim.minz(); z<=relabelim.maxz(); z++) 
    for (int y=relabelim.miny(); y<=relabelim.maxy(); y++) 
      for (int x=relabelim.minx(); x<=relabelim.maxx(); x++) 
	relabelim(x,y,z) = (T) newlabels[labelim(x,y,z)];
}

template <class T, class S>
void print_results(const vector<int>& idx, 
		   const vector<int>& size,
		   const vector<int>& pthreshindex,
		   const vector<float>& pvals,
		   const vector<float>& logpvals,
		   const vector<T>& maxvals,
		   const vector<triple<S> >& maxpos,
		   const vector<triple<float> >& cog,
		   const vector<T>& copemaxval,
		   const vector<triple<S> >& copemaxpos,
		   const vector<float>& copemean,
		   const volume<T>& zvol, const volume<T>& cope, 
		   const volume<int> &labelim)
{
  bool doAffineTransform=false;
  bool doWarpfieldTransform=false;
  volume4D<float>  full_field;
  int length=size.size();
  vector<triple<float> > fmaxpos, fcopemaxpos, fcog;
  copyconvert(maxpos,fmaxpos);
  copyconvert(cog,fcog);
  copyconvert(copemaxpos,fcopemaxpos);
  volume<T> stdvol;
  Matrix trans;
  const volume<T> *refvol = &zvol;
  if ( transformname.set() && stdvolname.set() ) {
    read_volume(stdvol,stdvolname.value());
    trans = read_ascii_matrix(transformname.value());
    if (verbose.value()) { 
      cout << "Transformation Matrix filename = "<<transformname.value()<<endl;
      cout << trans.Nrows() << " " << trans.Ncols() << endl; 
      cout << "Transformation Matrix = " << endl;
      cout << trans << endl;
    }
    doAffineTransform=true;
  }

  FnirtFileReader   reader;
  if (warpname.value().size()) 
  {
    reader.Read(warpname.value());
    full_field = reader.FieldAsNewimageVolume4D(true);
    doWarpfieldTransform=true;
  }

  if ( doAffineTransform || doWarpfieldTransform ) { 
    TransformToReference(fmaxpos,trans,zvol,stdvol,full_field,doAffineTransform,doWarpfieldTransform);
    TransformToReference(fcog,trans,zvol,stdvol,full_field,doAffineTransform,doWarpfieldTransform);
    if (copename.set()) TransformToReference(fcopemaxpos,trans,zvol,stdvol,full_field,doAffineTransform,doWarpfieldTransform);
  }

  if ( doAffineTransform ) refvol = &stdvol;
  if (mm.value()) {
    if (verbose.value()) { 
      cout << "Using matrix : " << endl << refvol->newimagevox2mm_mat() << endl; 
    }
    MultiplyCoordinateVector(fmaxpos,refvol->newimagevox2mm_mat());
    MultiplyCoordinateVector(fcog,refvol->newimagevox2mm_mat());
    if (copename.set()) MultiplyCoordinateVector(fcopemaxpos,refvol->newimagevox2mm_mat());  // used cope before
  } else {
    MultiplyCoordinateVector(fmaxpos,refvol->niftivox2newimagevox_mat().i());
    MultiplyCoordinateVector(fcog,refvol->niftivox2newimagevox_mat().i());
    if (copename.set()) MultiplyCoordinateVector(fcopemaxpos,refvol->niftivox2newimagevox_mat().i());   // used cope before
  }

  string units=" (vox)";
  if (mm.value()) units=" (mm)";
  string tablehead;
  tablehead = "Cluster Index\tVoxels";
  if (pthresh.set()) tablehead += "\tP\t-log10(P)";
  tablehead += "\tZ-MAX\tZ-MAX X" + units + "\tZ-MAX Y" + units + "\tZ-MAX Z" + units
    + "\tZ-COG X" + units + "\tZ-COG Y" + units + "\tZ-COG Z" + units;
  if (copename.set()) {
    tablehead+= "\tCOPE-MAX\tCOPE-MAX X" + units + "\tCOPE-MAX Y" + units + "\tCOPE-MAX Z" + units 
                 + "\tCOPE-MEAN";
  }

  if (!no_table.value()) cout << tablehead << endl;

  for (int n=length-1; n>=1; n--) {
    int index=idx[n];
    int k = size[index];
    float p = pvals[index];
    if ( !no_table.value() && pthreshindex[index]>0 ) {
      float mlog10p;
      mlog10p = -logpvals[index];
      cout << setprecision(3) << num(pthreshindex[index]) << "\t" << k << "\t"; 
      if (!pthresh.unset()) { cout << num(p) << "\t" << num(mlog10p) << "\t"; }
        cout << num(maxvals[index]) << "\t" 
	   << num(fmaxpos[index].x) << "\t" << num(fmaxpos[index].y) << "\t" 
	   << num(fmaxpos[index].z) << "\t"
	   << num(fcog[index].x) << "\t" << num(fcog[index].y) << "\t" 
	   << num(fcog[index].z);
      if (!copename.unset()) { 
	  cout   << "\t" << num(copemaxval[index]) << "\t"
	       << num(fcopemaxpos[index].x) << "\t" << num(fcopemaxpos[index].y) << "\t" 
	       << num(fcopemaxpos[index].z) << "\t" << num(copemean[index]);
	}
        cout << endl;
    }
  }

  // output local maxima
  if (outlmax.set() || outlmaxim.set()) {
    string outlmaxfile="/dev/null";
    if (outlmax.set()) { outlmaxfile=outlmax.value(); }
    ofstream lmaxfile(outlmaxfile.c_str());
    if (!lmaxfile)
      cerr << "Could not open file " << outlmax.value() << " for writing" << endl;
    lmaxfile << "Cluster Index\tZ\tx\ty\tz\t" << endl;
    volume<int> lmaxvol;
    copyconvert(zvol,lmaxvol);
    lmaxvol=0;
    zvol.setextrapolationmethod(zeropad);
    for (int n=size.size()-1; n>=1; n--) {
      int index=idx[n];
      if (pthreshindex[index]>0) {
	vector<int>   lmaxlistZ(size[index]);
	vector<triple<float> > lmaxlistR(size[index]);
	int lmaxlistcounter=0;
	for (int z=labelim.minz(); z<=labelim.maxz(); z++)
	  for (int y=labelim.miny(); y<=labelim.maxy(); y++)
	    for (int x=labelim.minx(); x<=labelim.maxx(); x++)
	      if ( checkIfLocalMaxima(index,labelim,zvol,numconnected.value(),x,y,z)) {
		lmaxvol(x,y,z)=1;
		lmaxlistZ[lmaxlistcounter]=(int)(1000.0*zvol(x,y,z));
		lmaxlistR[lmaxlistcounter].x=x;
		lmaxlistR[lmaxlistcounter].y=y;
		lmaxlistR[lmaxlistcounter].z=z;
		lmaxlistcounter++;
	      }

	lmaxlistZ.resize(lmaxlistcounter);
	vector<int> lmaxidx = get_sortindex(lmaxlistZ);
	if (peakdist.value()>0)
	{
	  for(int source=lmaxlistcounter-2;source>=0;source--)
	  {
	    vector<triple<float> > sourcecoord(1), erasecoord(1);
	    sourcecoord[0]=lmaxlistR[lmaxidx[source]];
	    erasecoord[0]=sourcecoord[0];
	    MultiplyCoordinateVector(sourcecoord,refvol->newimagevox2mm_mat());
	    for(int clust=lmaxlistcounter-1;clust>source;clust--)
	    {
	      vector<triple<float> > clustcoord(1);
	      clustcoord[0]=lmaxlistR[lmaxidx[clust]];
	      MultiplyCoordinateVector(clustcoord,refvol->newimagevox2mm_mat());
	      float dist = (sourcecoord[0].x-clustcoord[0].x)*(sourcecoord[0].x-clustcoord[0].x) + (sourcecoord[0].y-clustcoord[0].y )*(sourcecoord[0].y-clustcoord[0].y) + (sourcecoord[0].z-clustcoord[0].z)*(sourcecoord[0].z-clustcoord[0].z);
	      dist = sqrt(dist);
	      if (dist<peakdist.value()) {
		lmaxvol(MISCMATHS::round(erasecoord[0].x),
			MISCMATHS::round(erasecoord[0].y),
			MISCMATHS::round(erasecoord[0].z))=0;
		lmaxidx.erase(lmaxidx.begin()+source) ; 
		lmaxlistcounter--;	
		break;
	      }
	    }
	  }
	}

	// take a copy of coordinates in native space
	vector<triple<float> > lmaxlistRcopy(size[index]);
	lmaxlistRcopy = lmaxlistR;

	// transform coordinates (if requested)
	if ( doAffineTransform || doWarpfieldTransform ) TransformToReference(lmaxlistR,trans,zvol,stdvol,full_field,doAffineTransform,doWarpfieldTransform);
  
	if (mm.value()) MultiplyCoordinateVector(lmaxlistR,refvol->newimagevox2mm_mat());
	else MultiplyCoordinateVector(lmaxlistR,refvol->niftivox2newimagevox_mat().i());
	// display/store results
	for (int i=lmaxlistcounter-1; i>MISCMATHS::Max(lmaxlistcounter-1-mx_cnt.value(),-1); i--) 
	  {
	    lmaxfile << setprecision(3) << pthreshindex[index] << "\t" << lmaxlistZ[lmaxidx[i]]/1000.0 << "\t" << 
	      lmaxlistR[lmaxidx[i]].x << "\t" << lmaxlistR[lmaxidx[i]].y << "\t" << lmaxlistR[lmaxidx[i]].z << endl;
	  }
	// suppress the other local max in the output vol
	for (int i=MISCMATHS::Max(lmaxlistcounter-1-mx_cnt.value(),-1); i>-1; i--) 
	  {
	    lmaxvol(MISCMATHS::round(lmaxlistRcopy[lmaxidx[i]].x),
		    MISCMATHS::round(lmaxlistRcopy[lmaxidx[i]].y),
		    MISCMATHS::round(lmaxlistRcopy[lmaxidx[i]].z))=0;
	  }
	
      }
    }
    lmaxfile.close();
    if (outlmaxim.set()) {
      lmaxvol.setDisplayMaximumMinimum(0.0f,0.0f);
      save_volume(lmaxvol,outlmaxim.value());
    }
  }

}



template <class T>
int fmrib_main(int argc, char *argv[])
{
  volume<int> labelim;
  vector<int> size;
  vector<triple<int> > maxpos;
  vector<triple<float> > cog;
  vector<T> maxvals;
  vector<float> meanvals;
  float th = thresh.value();

  // read in the volume
  volume<T> zvol, mask, cope;
  read_volume(zvol,inputname.value());
  if (verbose.value())  print_volume_info(zvol,"Zvol");
  
  if ( fractional.value() ) {
    float frac = th;
    th = frac*(zvol.robustmax() - zvol.robustmin()) + zvol.robustmin();
  }
  mask = zvol;
  mask.binarise((T) th);
  if (minv.value()) { mask = ((T) 1) - mask; }
  if (verbose.value())  print_volume_info(mask,"Mask");
  
  // Find the connected components
  labelim = connected_components(mask,numconnected.value());
  if (verbose.value())  print_volume_info(labelim,"Labelim");
  
  // process according to the output format
  get_stats(labelim,zvol,size,maxvals,meanvals,maxpos,cog,minv.value());
  if (verbose.value()) {cout<<"Number of labels = "<<size.size()<<endl;}

  // process the cope image if entered
  vector<int> copesize;
  vector<triple<int> > copemaxpos;
  vector<triple<float> > copecog;
  vector<T> copemaxval;
  vector<float> copemean;
  if (!copename.unset()) {
    read_volume(cope,copename.value());
    get_stats(labelim,cope,copesize,copemaxval,copemean,copemaxpos,copecog,
	      minv.value());
  }

  // re-threshold for p
  int length = size.size();
  size[0] = 0;  // force background to be ordered last
  vector<int> pthreshsize;
  vector<float> pvals(length), logpvals(length);
  pthreshsize = size;
  int nozeroclust=0;
  if (!pthresh.unset()) {
    if (verbose.value()) 
      cout<<"Re-thresholding with p-value"<<endl;
    Infer infer(dLh.value(), th, voxvol.value());
    if (labelim.zsize()<=1) 
      infer.setD(2); // the 2D option
    if (minclustersize.value()) {
      float pmin=1.0;
      int nmin=0;
      while (pmin>=pthresh.value()) pmin=exp(infer(++nmin)); 
      cout << "Minimum cluster size under p-threshold = " << nmin << endl;
    }
    for (int n=1; n<length; n++) {
      int k = size[n];
      logpvals[n] = infer(k)/log(10);
      pvals[n] = exp(logpvals[n]*log(10));
      if (pvals[n]>pthresh.value()) {
	pthreshsize[n] = 0;
	nozeroclust++;
      }
    }
  }
  if (verbose.value()) {cout<<"Number of sub-p clusters = "<<nozeroclust<<endl;}

  // get sorted index (will revert to cluster size if no pthresh)
  vector<int> idx;
  idx = get_sortindex(pthreshsize);
  if (verbose.value()) {cout<<pthreshsize.size()<<" labels in sortedidx"<<endl;}

  vector<int> threshidx(length);
  for (int n=length-1; n>=1; n--) {
    int index=idx[n];
    if (pthreshsize[index]>0) 
      threshidx[index] = n - nozeroclust;
  }

  // print table
  print_results(idx, size, threshidx, pvals, logpvals, maxvals, maxpos, cog, copemaxval, copemaxpos, copemean, zvol, cope, labelim);
  
  labelim.setDisplayMaximumMinimum(0,0);
  // save relevant volumes
  if (!outindex.unset()) {
    volume<int> relabeledim;
    relabel_image(labelim,relabeledim,threshidx);
    save_volume(relabeledim,outindex.value());
  }
  if (!outsize.unset()) {
    volume<int> relabeledim;
    relabel_image(labelim,relabeledim,pthreshsize);
    save_volume(relabeledim,outsize.value());
  }
  if (!outmax.unset()) {
    volume<T> relabeledim;
    relabel_image(labelim,relabeledim,maxvals);
    save_volume(relabeledim,outmax.value());
  }
  if (!outmean.unset()) {
    volume<float> relabeledim;
    relabel_image(labelim,relabeledim,meanvals);
    save_volume(relabeledim,outmean.value());
  }
  if (!outpvals.unset()) {
    volume<float> relabeledim;
    relabel_image(labelim,relabeledim,logpvals);
    save_volume(relabeledim,outpvals.value());
  }
  if (!outthresh.unset()) {
    // Threshold the input volume st it is 0 for all non-clusters
    //   and maintains the same values otherwise
    volume<T> lcopy;
    relabel_image(labelim,lcopy,threshidx);
    lcopy.binarise(1);
    save_volume(lcopy*zvol,outthresh.value());
  }

  return 0;
}



extern "C" __declspec(dllexport) int _stdcall cluster(char *CmdLn)
{
  int argc;
  char **argv;
  
  parser(CmdLn, argc, argv);
  Tracer tr("main");

  OptionParser options(title, examples);

  try {
    options.add(inputname);
    options.add(thresh);
    options.add(outindex);
    options.add(outthresh);
    options.add(outlmax);
    options.add(outlmaxim);
    options.add(outsize);
    options.add(outmax);
    options.add(outmean);
    options.add(outpvals);
    options.add(pthresh);
    options.add(peakdist);
    options.add(copename);
    options.add(voxvol);
    options.add(dLh);
    options.add(fractional);
    options.add(numconnected);
    options.add(mm);
    options.add(minv);
    options.add(no_table);
    options.add(minclustersize);
    options.add(transformname);
    options.add(stdvolname);
    options.add(mx_cnt);
    options.add(verbose);
    options.add(help);
    options.add(warpname);
    
    options.parse_command_line(argc, argv);

    if ( (help.value()) || (!options.check_compulsory_arguments(true)) )
      {
	options.usage();
    freeparser(argc, argv);
    return(EXIT_FAILURE);
      }
    
    if ( (!pthresh.unset()) && (dLh.unset() || voxvol.unset()) ) 
      {
	options.usage();
	cerr << endl 
	     << "Both --dlh and --volume MUST be set if --pthresh is used." 
	     << endl;
    freeparser(argc, argv);
    return(EXIT_FAILURE);
      }
    
    if ( ( !transformname.unset() && stdvolname.unset() ) ||
	 ( transformname.unset() && (!stdvolname.unset()) ) ) 
      {
	options.usage();
	cerr << endl 
	     << "Both --xfm and --stdvol MUST be set together." 
	     << endl;
	freeparser(argc, argv);
	return(EXIT_FAILURE);
      }
  }  catch(X_OptionError& e) {
    options.usage();
    cerr << endl << e.what() << endl;
    freeparser(argc, argv);
    return(EXIT_FAILURE);
  } catch(std::exception &e) {
    cerr << e.what() << endl;
  } 

  int r=call_fmrib_main(dtype(inputname.value()),argc,argv);
  freeparser(argc, argv);
  return r;

}

}