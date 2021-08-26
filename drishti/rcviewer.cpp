#include "global.h"
#include "staticfunctions.h"
#include "rcviewer.h"
#include "geometryobjects.h"
#include "rcshaderfactory.h"
#include "matrix.h"
#include "dcolordialog.h"
#include "lighthandler.h"

#include <QProgressDialog>
#include <QInputDialog>


RcViewer::RcViewer() :
  QObject()
{
  m_Volume = 0;
  
  m_lut = 0;

  m_slcBuffer = 0;
  m_rboId = 0;
  m_slcTex[0] = 0;
  m_slcTex[1] = 0;
  m_slcTex[2] = 0;
  m_slcTex[3] = 0;

  m_glVertBuffer = 0;
  m_glIndexBuffer = 0;
  m_glVertArray = 0;

  m_mdEle = 0;
  m_mdCount = 0;
  m_mdIndices = 0;
  

  //m_blurShader = 0;
  m_ircShader = 0;
  m_eeShader = 0;

  m_mixTag = false;

  m_tagTex = 0;
  m_dataTex = 0;
  m_lutTex = 0;
  m_corner = Vec(0,0,0);
  m_vsize = Vec(1,1,1);
  m_sslevel = 1;

  m_filledTex = 0;
  m_ftBoxes = 0;

  m_maxRayLen = 100;
  m_shadow = 5.0;
  m_edge = 5.0;
  m_minGrad = 0;
  m_maxGrad = 20;
  
  m_renderMode = 1; // 0-point, 1-raycast, 2-xray

  m_crops.clear();

  m_amrData = false;
  m_amrTex = 0;
  

  m_flhist1D = new float[256];
  m_flhist2D = new float[256*256];
  m_subvolume1dHistogram = new int[256];
  m_subvolume2dHistogram = new int[256*256];
  memset(m_subvolume1dHistogram, 0, 256*4);
  memset(m_subvolume2dHistogram, 0, 256*256*4);


  connect(&m_boundingBox, SIGNAL(updated()),
	  this, SLOT(boundingBoxChanged()));


  //m_boxHistogram.clear();

  init();
}

RcViewer::~RcViewer()
{
  init();
}

void
RcViewer::setBrickInfo(QList<BrickInformation> binfo)
{
  m_brickInfo = binfo;
}

void
RcViewer::init()
{
  m_skipLayers = 0;
  m_skipVoxels = 0;
  m_fullRender = false;
  m_dragMode = true;

  m_dbox = m_wbox = m_hbox = 0;
  m_boxSize = 8;
  m_boxMinMax.clear();
  if (m_ftBoxes) delete [] m_ftBoxes;
  m_ftBoxes = 0;
  m_filledBoxes.clear();

  m_numBoxes = 0;
  
//  if (m_boxHistogram.count() > 0)
//    {
//      for(int b=0; b<m_boxHistogram.count(); b++)
//	delete [] m_boxHistogram[b];
//      m_boxHistogram.clear();
//    }
  
  m_bytesPerVoxel = 1;
  m_volPtr = 0;
  m_vfm = 0;

  if (m_ircShader)
    glDeleteObjectARB(m_ircShader);
  m_ircShader = 0;

  if (m_eeShader)
    glDeleteObjectARB(m_eeShader);
  m_eeShader = 0;

//  if (m_blurShader)
//    glDeleteObjectARB(m_blurShader);
//  m_blurShader = 0;

  if (m_slcBuffer) glDeleteFramebuffers(1, &m_slcBuffer);
  if (m_rboId) glDeleteRenderbuffers(1, &m_rboId);
  if (m_slcTex[0]) glDeleteTextures(5, m_slcTex);
  m_slcBuffer = 0;
  m_rboId = 0;
  m_slcTex[0] = 0;
  m_slcTex[1] = 0;
  m_slcTex[2] = 0;
  m_slcTex[3] = 0;

  if (m_dataTex) glDeleteTextures(1, &m_dataTex);
  m_dataTex = 0;

  if (m_lutTex) glDeleteTextures(1, &m_lutTex);
  m_lutTex = 0;

  if (m_filledTex) glDeleteTextures(1, &m_filledTex);
  m_filledTex = 0;

  m_corner = Vec(0,0,0);
  m_vsize = Vec(1,1,1);
  m_sslevel = 1;

  m_mixTag = false;

  m_crops.clear();

  if(m_glVertArray)
    {
      glDeleteBuffers(1, &m_glIndexBuffer);
      glDeleteVertexArrays( 1, &m_glVertArray );
      glDeleteBuffers(1, &m_glVertBuffer);
      m_glIndexBuffer = 0;
      m_glVertArray = 0;
      m_glVertBuffer = 0;
    }
  
  m_mdEle = 0;
  if (m_mdCount) delete [] m_mdCount;
  if (m_mdIndices) delete [] m_mdIndices;  
  m_mdCount = 0;
  m_mdIndices = 0;

}

void RcViewer::setLut(uchar *l) { m_lut = l; }

void
RcViewer::setVolDataPtr(VolumeFileManager *ptr)
{
  m_vfm = ptr;

  if (!m_vfm)
    return;
  
  m_bytesPerVoxel = m_vfm->bytesPerVoxel();

  m_boxMinMax.clear();
  m_filledBoxes.clear();
  if (m_ftBoxes) delete [] m_ftBoxes;
  m_ftBoxes = 0;
}

void
RcViewer::resizeGL(int width, int height)
{
  if (m_vfm)
    createFBO();
}

void
RcViewer::setGridSize(int d, int w, int h)
{
  if (!m_vfm)
    return;
  
  m_depth = d;
  m_width = w;
  m_height = h;

  m_boxSize = qMax((int)m_height/128, (int)m_width/128);
  m_boxSize = qMax(m_boxSize, (int)m_depth/128);
  m_boxSize = qMax(m_boxSize, 8);

  m_dataMin = Vec(0,0,0);
  m_dataMax = Vec(h,w,d);

  m_minDSlice = 0;
  m_minWSlice = 0;
  m_minHSlice = 0;
  m_maxDSlice = m_depth;
  m_maxWSlice = m_width;
  m_maxHSlice = m_height;  

  m_cminD = 0;
  m_cminW = 0;
  m_cminH = 0;
  m_cmaxD = m_depth;
  m_cmaxW = m_width;
  m_cmaxH = m_height;  

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_max2DTexSize);
  //glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &m_max3DTexSize);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &m_max3DTexSize);

  createShaders();

  m_boundingBox.setBounds(Vec(0,0,0),
			  Vec(m_maxHSlice,
			      m_maxWSlice,
			      m_maxDSlice));
}

void
RcViewer::updateSubvolume(Vec bmin, Vec bmax)
{
  if (!m_vfm)
    return;

  m_dataMin = bmin;
  m_dataMax = bmax;

  m_minDSlice = bmin.z;
  m_maxDSlice = bmax.z;
  m_minWSlice = bmin.y;
  m_maxWSlice = bmax.y;
  m_minHSlice = bmin.x;
  m_maxHSlice = bmax.x;  
  
  Vec voxelScaling = Global::voxelScaling();

  Vec bbmax = Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice);
  bbmax = VECPRODUCT(bbmax, voxelScaling);
  m_boundingBox.setBounds(Vec(0,0,0), bbmax);


  bmax = VECPRODUCT(bmax, voxelScaling);
  bmin = VECPRODUCT(bmin, voxelScaling);
  m_boundingBox.setPositions(bmin, bmax);
}

void
RcViewer::loadLookupTable()
{
  if (!m_vfm)
    return;

  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_lutTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,
	       0, // single resolution
	       GL_RGBA,
	       256, Global::lutSize()*256, // width, height
	       0, // no border
	       GL_RGBA,
	       GL_UNSIGNED_BYTE,
	       m_lut);
  glDisable(GL_TEXTURE_2D);

  updateFilledBoxes();  
}

void
RcViewer::generateBoxMinMax()
{
  QProgressDialog progress(QString("Updating min-max structure (%1)").arg(m_boxSize),
			   QString(),
			   0, 100,
			   0);
  progress.setMinimumDuration(0);

  m_dbox = m_depth/m_boxSize;
  m_wbox = m_width/m_boxSize;
  m_hbox = m_height/m_boxSize;
  if (m_depth%m_boxSize > 0) m_dbox++;
  if (m_width%m_boxSize > 0) m_wbox++;
  if (m_height%m_boxSize > 0) m_hbox++;

  m_boxMinMax.clear();
  m_boxMinMax.reserve(2*m_dbox*m_wbox*m_hbox);

  m_filledBoxes.resize(m_dbox*m_wbox*m_hbox);
  m_filledBoxes.fill(false);

  if (m_ftBoxes) delete [] m_ftBoxes;
  m_ftBoxes = new uchar[m_dbox*m_wbox*m_hbox];
  memset(m_ftBoxes, 0, m_dbox*m_wbox*m_hbox);

  for(int d=0; d<m_dbox; d++)
    {
      progress.setValue(100*(float)d/m_dbox);
      qApp->processEvents();

      for(int w=0; w<m_wbox; w++)
	for(int h=0; h<m_hbox; h++)
	  {
	    int vmax = -1;
	    int vmin = 65535;
	    int dmin = d*m_boxSize;
	    int wmin = w*m_boxSize;
	    int hmin = h*m_boxSize;
	    int dmax = qMin((d+1)*m_boxSize, (int)m_depth);
	    int wmax = qMin((w+1)*m_boxSize, (int)m_width);
	    int hmax = qMin((h+1)*m_boxSize, (int)m_height);
	    if (m_bytesPerVoxel == 1)
	      {
		for(int dm=dmin; dm<dmax; dm++)
		  for(int wm=wmin; wm<wmax; wm++)
		    for(int hm=hmin; hm<hmax; hm++)
		      {
			int v = m_volPtr[dm*m_width*m_height + wm*m_height + hm];
			vmin = qMin(vmin, v);
			vmax = qMax(vmax, v);
		      }
	      }
	    else
	      {
		ushort *vPtr = (ushort*)m_volPtr;
		for(int dm=dmin; dm<dmax; dm++)
		  for(int wm=wmin; wm<wmax; wm++)
		    for(int hm=hmin; hm<hmax; hm++)
		      {
			int v = vPtr[dm*m_width*m_height + wm*m_height + hm];
			vmin = qMin(vmin, v);
			vmax = qMax(vmax, v);
		      }
	      }
	    m_boxMinMax << vmin;
	    m_boxMinMax << vmax;
	  }
    }

  progress.setValue(100);
}

void
RcViewer::dummyDraw()
{
  GeometryObjects::crops()->postdraw(m_viewer);
  GeometryObjects::clipplanes()->postdraw(m_viewer);
  GeometryObjects::hitpoints()->postdraw(m_viewer);
  GeometryObjects::paths()->postdraw(m_viewer);

  ClipInformation clipInfo; // dummy
  GeometryObjects::scalebars()->draw(m_viewer, clipInfo);
 
  if (Global::bottomText())  
    drawInfo();
}

void
RcViewer::fastDraw()
{
  m_dragMode = true;

  if (!m_volPtr)
    return;

  raycasting();

//  glEnable(GL_BLEND);
//  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // blend on top

  GeometryObjects::crops()->postdraw(m_viewer);
  GeometryObjects::clipplanes()->postdraw(m_viewer);
  GeometryObjects::hitpoints()->postdraw(m_viewer);
  GeometryObjects::paths()->postdraw(m_viewer);

  ClipInformation clipInfo; // dummy
  GeometryObjects::scalebars()->draw(m_viewer, clipInfo);
 
  if (Global::bottomText())  
    drawInfo();
}


void
RcViewer::draw()
{
  m_dragMode = false;

  if (!m_volPtr)
    return;

  raycasting();

//  glEnable(GL_BLEND);
//  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // blend on top

  GeometryObjects::crops()->postdraw(m_viewer);
  GeometryObjects::clipplanes()->postdraw(m_viewer);
  GeometryObjects::hitpoints()->postdraw(m_viewer);
  GeometryObjects::paths()->postdraw(m_viewer);

  ClipInformation clipInfo; // dummy
  GeometryObjects::scalebars()->draw(m_viewer, clipInfo);


  if (Global::bottomText())  
    drawInfo();
}

void
RcViewer::createFBO()
{
  int wd = m_viewer->camera()->screenWidth();
  int ht = m_viewer->camera()->screenHeight();

  //-----------------------------------------
  if (m_slcBuffer) glDeleteFramebuffers(1, &m_slcBuffer);
  if (m_rboId) glDeleteRenderbuffers(1, &m_rboId);
  if (m_slcTex[0]) glDeleteTextures(5, m_slcTex);  

  glGenFramebuffers(1, &m_slcBuffer);
  glGenRenderbuffers(1, &m_rboId);
  glGenTextures(4, m_slcTex);

  glBindFramebuffer(GL_FRAMEBUFFER, m_slcBuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_rboId);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, wd, ht);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // attach the renderbuffer to depth attachment point
  glFramebufferRenderbuffer(GL_FRAMEBUFFER,      // 1. fbo target: GL_FRAMEBUFFER
			    GL_DEPTH_ATTACHMENT, // 2. attachment point
			    GL_RENDERBUFFER,     // 3. rbo target: GL_RENDERBUFFER
			    m_rboId);            // 4. rbo ID

  for(int i=0; i<4; i++)
    {
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[i]);
      glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,
		   0,
		   GL_RGBA32F,
		   wd, ht,
		   0,
		   GL_RGBA,
		   GL_FLOAT,
		   0);
    }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  //-----------------------------------------
}

void
RcViewer::createIsoRaycastShader()
{
  QString shaderString;

  shaderString = RcShaderFactory::genRaycastShader(m_crops, m_bytesPerVoxel==2,
						   m_amrData);

  m_ircShader = glCreateProgramObjectARB();
  if (! RcShaderFactory::loadShader(m_ircShader,
				    shaderString))
    {
      m_ircShader = 0;
      QMessageBox::information(0, "", "Cannot create surface shader.");
    }
  
  m_ircParm[0] = glGetUniformLocationARB(m_ircShader, "dataTex");
  m_ircParm[1] = glGetUniformLocationARB(m_ircShader, "lutTex");
  m_ircParm[2] = glGetUniformLocationARB(m_ircShader, "exitTex");
  m_ircParm[3] = glGetUniformLocationARB(m_ircShader, "entryTex");
  m_ircParm[4] = glGetUniformLocationARB(m_ircShader, "tagTex");
  m_ircParm[5] = glGetUniformLocationARB(m_ircShader, "stepSize");
  m_ircParm[6] = glGetUniformLocationARB(m_ircShader, "eyepos");
  m_ircParm[7] = glGetUniformLocationARB(m_ircShader, "vcorner");
  m_ircParm[8] = glGetUniformLocationARB(m_ircShader, "vsize");
  m_ircParm[9] = glGetUniformLocationARB(m_ircShader, "voxelScale");
  m_ircParm[10]= glGetUniformLocationARB(m_ircShader, "saveCoord");
  m_ircParm[11]= glGetUniformLocationARB(m_ircShader, "skipLayers");
  m_ircParm[12]= glGetUniformLocationARB(m_ircShader, "skipVoxels");
  m_ircParm[13]= glGetUniformLocationARB(m_ircShader, "mixTag");
  m_ircParm[14]= glGetUniformLocationARB(m_ircShader, "nclip");
  m_ircParm[15]= glGetUniformLocationARB(m_ircShader, "clipPos");
  m_ircParm[16]= glGetUniformLocationARB(m_ircShader, "clipNormal");
  m_ircParm[17]= glGetUniformLocationARB(m_ircShader, "maxDeep");
  m_ircParm[18]= glGetUniformLocationARB(m_ircShader, "minGrad");
  m_ircParm[19]= glGetUniformLocationARB(m_ircShader, "maxGrad");
  m_ircParm[20]= glGetUniformLocationARB(m_ircShader, "sslevel");
  m_ircParm[21]= glGetUniformLocationARB(m_ircShader, "MVP");
  m_ircParm[22]= glGetUniformLocationARB(m_ircShader, "gamma");
  m_ircParm[23]= glGetUniformLocationARB(m_ircShader, "interp");


  m_ircParm[34] = glGetUniformLocationARB(m_ircShader, "lightTex");
  m_ircParm[35] = glGetUniformLocationARB(m_ircShader, "lightgridx");
  m_ircParm[36] = glGetUniformLocationARB(m_ircShader, "lightgridy");
  m_ircParm[37] = glGetUniformLocationARB(m_ircShader, "lightgridz");
  m_ircParm[38] = glGetUniformLocationARB(m_ircShader, "lightnrows");
  m_ircParm[39] = glGetUniformLocationARB(m_ircShader, "lightncols");
  m_ircParm[40] = glGetUniformLocationARB(m_ircShader, "lightlod");

  if (m_amrData)
    m_ircParm[41] = glGetUniformLocationARB(m_ircShader, "amrTex");

  m_ircParm[42] = glGetUniformLocationARB(m_ircShader, "ambient");
  m_ircParm[43] = glGetUniformLocationARB(m_ircShader, "diffuse");
  m_ircParm[44] = glGetUniformLocationARB(m_ircShader, "roughness");
  m_ircParm[45] = glGetUniformLocationARB(m_ircShader, "specular");

  m_ircParm[46] = glGetUniformLocationARB(m_ircShader, "viewDir");
}

void
RcViewer::createShaders()
{
  QString shaderString;

  createIsoRaycastShader();

  shaderString = RcShaderFactory::genEdgeEnhanceShader();

  m_eeShader = glCreateProgramObjectARB();
  if (! RcShaderFactory::loadShader(m_eeShader,
				  shaderString))
    {
      m_eeShader = 0;
      QMessageBox::information(0, "", "Cannot create ee shader.");
    }

  m_eeParm[0] = glGetUniformLocationARB(m_eeShader, "normalTex");
  m_eeParm[1] = glGetUniformLocationARB(m_eeShader, "pvtTex");
  m_eeParm[2] = glGetUniformLocationARB(m_eeShader, "sceneRadius");
  m_eeParm[4] = glGetUniformLocationARB(m_eeShader, "dzScale");
  m_eeParm[5] = glGetUniformLocationARB(m_eeShader, "isoshadow");
  m_eeParm[6] = glGetUniformLocationARB(m_eeShader, "gamma");
  m_eeParm[7] = glGetUniformLocationARB(m_eeShader, "roughness");
  m_eeParm[8] = glGetUniformLocationARB(m_eeShader, "specular");
//----------------------



//  //----------------------
//  shaderString = RcShaderFactory::genRectBlurShaderString(1); // bilateral filter
//
//  m_blurShader = glCreateProgramObjectARB();
//  if (! RcShaderFactory::loadShader(m_blurShader,
//				  shaderString))
//    {
//      m_blurShader = 0;
//      QMessageBox::information(0, "", "Cannot create blur shader.");
//    }
//
//  m_blurParm[0] = glGetUniformLocationARB(m_blurShader, "blurTex");
//  m_blurParm[1] = glGetUniformLocationARB(m_blurShader, "minZ");
//  m_blurParm[2] = glGetUniformLocationARB(m_blurShader, "maxZ");
//  //----------------------
}


void
RcViewer::updateVoxelsForRaycast(GLuint *dataTex)
{

  if (!m_vfm)
    return;

  if (m_filledBoxes.count() == 0)
    {      
      m_vfm->setMemMapped(true);
      m_vfm->loadMemFile();
      m_volPtr = m_vfm->memVolDataPtr();
      if (m_volPtr)
	generateBoxMinMax();
      
      // generate required structures
      generateBoxes();
      
      loadAllBoxesToVBO();  
    }
  
  if (!m_volPtr)
    return;

  if (!m_lutTex) glGenTextures(1, &m_lutTex);

  { // update box for voxel upload
    Vec voxelScaling = Global::voxelScaling();
    Vec bmin, bmax;
    m_boundingBox.bounds(bmin, bmax);
    bmin = VECDIVIDE(bmin, voxelScaling);
    bmax = VECDIVIDE(bmax, voxelScaling);
    m_minDSlice = bmin.z;
    m_minWSlice = bmin.y;
    m_minHSlice = bmin.x;
    m_maxDSlice = qCeil(bmax.z); // because we are getting it as float
    m_maxWSlice = qCeil(bmax.y);
    m_maxHSlice = qCeil(bmax.x);
  }
      
  m_sslevel = m_Volume->getSubvolumeSubsamplingLevel();
  Vec vsz = m_Volume->getSubvolumeTextureSize();
  qint64 hsz = vsz.x;
  qint64 wsz = vsz.y;
  qint64 dsz = vsz.z;
  m_corner = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  m_vsize = Vec(hsz, wsz, dsz);

  m_dataTex = dataTex[1];
  glActiveTexture(GL_TEXTURE1);
  glEnable(GL_TEXTURE_2D_ARRAY);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_dataTex);
  glDisable(GL_TEXTURE_2D_ARRAY);


  memcpy(m_subvolume1dHistogram, m_Volume->getSubvolume1dHistogram(0), 256*4);
  memcpy(m_subvolume2dHistogram, m_Volume->getSubvolume2dHistogram(0), 256*256*4);
}

void
RcViewer::activateBounds(bool b)
{
  if (b)
    m_boundingBox.activateBounds();
  else
    m_boundingBox.deactivateBounds();
}

void
RcViewer::drawInfo()
{
  glDisable(GL_DEPTH_TEST);

  QFont tfont = QFont("Helvetica", 12);  
  QString mesg;

//  mesg += QString("LOD(%1) Vol(%2 %3 %4) ").				\
//    arg(m_sslevel).arg(m_vsize.x).arg(m_vsize.y).arg(m_vsize.z);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_POINT_SMOOTH);

  int wd = m_viewer->camera()->screenWidth();
  int ht = m_viewer->camera()->screenHeight();
  StaticFunctions::pushOrthoView(0, 0, wd, ht);
//  StaticFunctions::renderText(10,10, mesg, tfont, Qt::black, Qt::lightGray);

  tfont = QFont("Helvetica", 12);  
  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);
  float step = Global::stepsizeStill();
  mesg = QString("box(%1 %2 %3 : %4 %5 %6 : %7 %8 %9) step(%10)").	\
    arg(bmin.x).arg(bmin.y).arg(bmin.z).				\
    arg(bmax.x).arg(bmax.y).arg(bmax.z).				\
    arg(bmax.x-bmin.x).arg(bmax.y-bmin.y).arg(bmax.z-bmin.z).\
    arg(step);
//  float vszgb = (bmax.x-bmin.x)*(bmax.y-bmin.y)*(bmax.z-bmin.z);
//  vszgb /= m_sslevel;
//  vszgb /= m_sslevel;
//  vszgb /= m_sslevel;
//  vszgb /= 1024;
//  vszgb /= 1024;
//  mesg += QString("mb(%1 @ %2)").arg(vszgb).arg(m_sslevel);
//  StaticFunctions::renderText(10,30, mesg, tfont, Qt::black, Qt::lightGray);

  StaticFunctions::renderText(10,10, mesg, tfont, Qt::black, Qt::lightGray);

  StaticFunctions::popOrthoView();

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
}

void
RcViewer::raycasting()
{
  Vec voxelScaling = Global::voxelScaling();

  Vec bminO, bmaxO;
  m_boundingBox.bounds(bminO, bmaxO);

  bminO = VECPRODUCT(bminO, voxelScaling);
  bmaxO = VECPRODUCT(bmaxO, voxelScaling);

  Vec pivot = m_brickInfo[0].getCorrectedPivot();
  Vec axis = m_brickInfo[0].axis;
  float angle = m_brickInfo[0].angle;
  pivot = VECPRODUCT((Vec(1,1,1)-pivot),bminO) + VECPRODUCT(pivot,bmaxO);
  m_b0xform.setToIdentity();
  m_b0xform.translate(pivot.x, pivot.y, pivot.z);
  m_b0xform.rotate(angle, axis.x, axis.y, axis.z);
  m_b0xform.translate(-pivot.x, -pivot.y, -pivot.z);

  QMatrix4x4 b0xform;
  b0xform = m_b0xform.inverted();
  
  QVector3D box[8];
  box[0] = QVector3D(bminO.x, bminO.y, bminO.z);
  box[1] = QVector3D(bminO.x, bminO.y, bmaxO.z);
  box[2] = QVector3D(bminO.x, bmaxO.y, bminO.z);
  box[3] = QVector3D(bminO.x, bmaxO.y, bmaxO.z);
  box[4] = QVector3D(bmaxO.x, bminO.y, bminO.z);
  box[5] = QVector3D(bmaxO.x, bminO.y, bmaxO.z);
  box[6] = QVector3D(bmaxO.x, bmaxO.y, bminO.z);
  box[7] = QVector3D(bmaxO.x, bmaxO.y, bmaxO.z);


  Vec eyepos = m_viewer->camera()->position();
  QVector3D e = QVector3D(b0xform.map(QVector4D(eyepos.x, eyepos.y, eyepos.z, 1.0)));
  
  float minZ = 1000000;
  float maxZ = -1000000;
  for(int b=0; b<8; b++)
    {
      float zv = (box[b]-e).length();
      minZ = qMin(minZ, zv);
      maxZ = qMax(maxZ, zv);
    }
  
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);


  checkCrops();

  float sceneRadius = maxZ-minZ;
  raycast(Vec(e.x(), e.y(), e.z()), sceneRadius, false); // raycast surface process
}


void
RcViewer::raycast(Vec eyepos, float sceneRadius, bool firstPartOnly)
{
  Vec voxelScaling = Global::voxelScaling();

  Vec bgColor = Global::backgroundColor();
 
  Vec viewDir = m_viewer->camera()->viewDirection();
  Vec subvolcorner = Vec(m_minHSlice, m_minWSlice, m_minDSlice);
  Vec subvolsize = Vec(m_maxHSlice-m_minHSlice+1,
		       m_maxWSlice-m_minWSlice+1,
		       m_maxDSlice-m_minDSlice+1);
  

  glClearDepth(0);
  glClearColor(0, 0, 0, 0);

  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
  for(int fbn=0; fbn<2; fbn++)
    {
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			     GL_COLOR_ATTACHMENT0_EXT,
			     GL_TEXTURE_RECTANGLE_ARB,
			     m_slcTex[fbn],
			     0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);

  //----------------------------
  // create exit points
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			 GL_COLOR_ATTACHMENT0_EXT,
			 GL_TEXTURE_RECTANGLE_ARB,
			 m_slcTex[1],
			 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);  
  glDepthFunc(GL_GEQUAL);
  drawVBOBox(GL_FRONT);
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  //----------------------------
  
  //----------------------------
  // create entry points
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			 GL_COLOR_ATTACHMENT0_EXT,
			 GL_TEXTURE_RECTANGLE_ARB,
			 m_slcTex[0],
			 0);
  glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
  glClearDepth(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDepthFunc(GL_LEQUAL);
  drawVBOBox(GL_BACK);
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  //----------------------------

  //----------------------------
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);

  
  if (!firstPartOnly)
    {
      glBindFramebuffer(GL_FRAMEBUFFER_EXT, m_slcBuffer);

      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			     GL_COLOR_ATTACHMENT0_EXT,
			     GL_TEXTURE_RECTANGLE_ARB,
			     m_slcTex[2],
			     0);
      
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
			     GL_COLOR_ATTACHMENT1_EXT,
			     GL_TEXTURE_RECTANGLE_ARB,
			     m_slcTex[3],
			     0);
      
      GLenum buffers[2] = { GL_COLOR_ATTACHMENT0_EXT,
			    GL_COLOR_ATTACHMENT1_EXT };
      glDrawBuffersARB(2, buffers);
    }
  
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //----------------------------


  float stepsize = Global::stepsizeStill();
 
  stepsize /= qMax(m_vsize.x,qMax(m_vsize.y,m_vsize.z));

  //---------
  //---------
  glUseProgramObjectARB(m_ircShader);
  glUniform1iARB(m_ircParm[0], 1); // dataTex
  glUniform1iARB(m_ircParm[1], 0); // lutTex
  glUniform1iARB(m_ircParm[2], 2); // slcTex[1] - contains exit coordinates
  glUniform1iARB(m_ircParm[3], 6); // slcTex[0] - contains entry coordinates
  glUniform1iARB(m_ircParm[4], 5); // tagTex
  glUniform1fARB(m_ircParm[5], stepsize); // stepSize
  glUniform3fARB(m_ircParm[6], eyepos.x, eyepos.y, eyepos.z); // eyepos
  glUniform3fARB(m_ircParm[7], subvolcorner.x, subvolcorner.y, subvolcorner.z);
  glUniform3fARB(m_ircParm[8], m_sslevel*m_vsize.x, m_sslevel*m_vsize.y, m_sslevel*m_vsize.z);
  glUniform3fARB(m_ircParm[9], voxelScaling.x, voxelScaling.y, voxelScaling.z);
  glUniform1iARB(m_ircParm[10],firstPartOnly); // save voxel coordinates
  glUniform1iARB(m_ircParm[11],m_skipLayers); // skip first layers
  glUniform1iARB(m_ircParm[12],m_skipVoxels); // skip first voxels
  glUniform1iARB(m_ircParm[13],m_mixTag); // mixTag

  { // apply clip planes to modify entry and exit points
    QList<Vec> cPos;
    QList<Vec> cNorm;
    //cPos << m_viewer->camera()->position();
    cPos << m_viewer->camera()->position()+100*m_viewer->camera()->viewDirection();
    cNorm << -m_viewer->camera()->viewDirection();
    //cPos += GeometryObjects::clipplanes()->positions();
    //cNorm += GeometryObjects::clipplanes()->normals();

    QList<Vec> bcPos = GeometryObjects::clipplanes()->positions();
    QList<Vec> bcNorm = GeometryObjects::clipplanes()->normals();
    QList<bool> clippers;
    clippers = m_brickInfo[0].clippers;
    for(int ci=0; ci<clippers.count(); ci++)
      {
	if (clippers[ci])
	  {
	    cPos << bcPos[ci];
	    cNorm << bcNorm[ci];
	  }
      }
    int nclip = cPos.count();
    float cpos[100];
    float cnormal[100];
    for(int c=0; c<nclip; c++)
      {
	cpos[3*c+0] = cPos[c].x*voxelScaling.x;
	cpos[3*c+1] = cPos[c].y*voxelScaling.y;
	cpos[3*c+2] = cPos[c].z*voxelScaling.z;
      }
    for(int c=0; c<nclip; c++)
      {
	cnormal[3*c+0] = -cNorm[c].x;
	cnormal[3*c+1] = -cNorm[c].y;
	cnormal[3*c+2] = -cNorm[c].z;
      }
    glUniform1i(m_ircParm[14], nclip); // clipplanes
    glUniform3fv(m_ircParm[15], nclip, cpos); // clipplanes
    glUniform3fv(m_ircParm[16], nclip, cnormal); // clipplanes
  }

  //glUniform1f(m_ircParm[17], qPow(2.0f,0.1f*m_maxRayLen)); // go m_deep voxels deep
  glUniform1f(m_ircParm[17], 0.01f*m_maxRayLen); // go m_deep voxels deep

  glUniform1f(m_ircParm[18], m_minGrad*0.05); // min gradient magnitute
  glUniform1f(m_ircParm[19], m_maxGrad*0.05); // max gradient magnitute

  glUniform1fARB(m_ircParm[20], m_sslevel);

  { // feed model-view-projection matrix for depth calculations
    GLdouble m[16];
    GLfloat mvp[16];
    m_viewer->camera()->getModelViewProjectionMatrix(m);
    for(int i=0; i<16; i++) mvp[i] = m[i];
    
    glUniformMatrix4fv(m_ircParm[21], 1, GL_FALSE, mvp);
  }
  //---------
  //---------

  //==============================
  // for lighting
  //==============================
  glUniform1iARB(m_ircParm[34], 4); // lightTex
  glActiveTexture(GL_TEXTURE4);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, LightHandler::texture());
  glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (!LightHandler::basicLight())
    {
      int lightgridx, lightgridy, lightgridz, lightncols, lightnrows, lightlod;
      LightHandler::lightBufferInfo(lightgridx, lightgridy, lightgridz,
				    lightnrows, lightncols, lightlod);
      Vec draginfo = m_Volume->getDragTextureInfo();
      int lod = m_Volume->getSubvolumeSubsamplingLevel();
      float plod = draginfo.z/float(lod);
      lightlod *= plod;
      
      glUniform1iARB(m_ircParm[35], lightgridx); // lightgridx
      glUniform1iARB(m_ircParm[36], lightgridy); // lightgridy
      glUniform1iARB(m_ircParm[37], lightgridz); // lightgridz
      glUniform1iARB(m_ircParm[38], lightnrows); // lightnrows
      glUniform1iARB(m_ircParm[39], lightncols); // lightncols
      glUniform1iARB(m_ircParm[40], lightlod); // lightlod
    }
  else
    {
      // lightlod 0 means use basic lighting model
      int lightlod = 0;
      glUniform1iARB(m_ircParm[40], lightlod); // lightlod
    }
  glUniform1fARB(m_ircParm[22], Global::gamma()); // gamma
  //==============================
  //==============================

  float interp = 1;
  if (m_mixTag || !Global::interpolationType(Global::TextureInterpolation))
    interp = 0;
  
  glUniform1fARB(m_ircParm[23], interp); // interpolate

  if (m_mixTag)
    {
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_1D, m_tagTex);
      glEnable(GL_TEXTURE_1D);
    }

//  glActiveTexture(GL_TEXTURE3);
//  glEnable(GL_TEXTURE_3D);
//  glBindTexture(GL_TEXTURE_3D, m_filledTex);

  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[1]);

  glActiveTexture(GL_TEXTURE6);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[0]);

  glActiveTexture(GL_TEXTURE1);
  glEnable(GL_TEXTURE_2D_ARRAY);
  glBindTexture(GL_TEXTURE_2D_ARRAY, m_dataTex);

  if (m_amrData)
    {
      glUniform1iARB(m_ircParm[41], 7); // dataTex
      glActiveTexture(GL_TEXTURE7);
      glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_amrTex);
      glEnable(GL_TEXTURE_RECTANGLE_ARB);
    }
  
  glUniform1fARB(m_ircParm[42], m_ambient); // ambient
  glUniform1fARB(m_ircParm[43], m_diffuse); // diffuse
  glUniform1fARB(m_ircParm[44], 0.9-m_roughness*0.1); // roughness
  glUniform1fARB(m_ircParm[45], m_specular); // specular


  Vec shdDir = (eyepos - m_viewer->sceneCenter()).unit();
  shdDir -= m_viewer->camera()->rightVector();
  shdDir += m_viewer->camera()->upVector();
  shdDir.normalize();
  glUniform3fARB(m_ircParm[46], shdDir.x, shdDir.y, shdDir.z);
    
  if (firstPartOnly ||
      !Global::interpolationType(Global::TextureInterpolation)) // linear
    {
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
  else
    {
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_lutTex);

  int wd = m_viewer->camera()->screenWidth();
  int ht = m_viewer->camera()->screenHeight();
  StaticFunctions::pushOrthoView(0, 0, wd, ht);
  StaticFunctions::drawQuad(0, 0, wd, ht, 1);
  StaticFunctions::popOrthoView();

  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  //----------------------------

  // light texture
  glActiveTexture(GL_TEXTURE4);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  
  glActiveTexture(GL_TEXTURE6);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

//  glActiveTexture(GL_TEXTURE3);
//  glDisable(GL_TEXTURE_3D);

  if (m_amrData)
    {
      glActiveTexture(GL_TEXTURE7);
      glDisable(GL_TEXTURE_RECTANGLE_ARB);
    }

  
  if (firstPartOnly)
    {
      glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
      glUseProgramObjectARB(0);
      
      if (m_mixTag)
	{
	  glActiveTexture(GL_TEXTURE5);
	  glDisable(GL_TEXTURE_1D);
	}

      glActiveTexture(GL_TEXTURE0);
      glDisable(GL_TEXTURE_2D);
      
      glActiveTexture(GL_TEXTURE1);
      //glDisable(GL_TEXTURE_3D);
      glDisable(GL_TEXTURE_2D_ARRAY);
      
      glActiveTexture(GL_TEXTURE2);
      glDisable(GL_TEXTURE_RECTANGLE_ARB);

      return;
    }
  

  glClearColor(bgColor.x, bgColor.y, bgColor.z, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  //--------------------------------
  glUseProgramObjectARB(m_eeShader);
  
  glActiveTexture(GL_TEXTURE6);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[3]);
  
  glActiveTexture(GL_TEXTURE2);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_slcTex[2]);
   
  glUniform1iARB(m_eeParm[0], 6); // normals (slctex[3]) tex
  glUniform1iARB(m_eeParm[1], 2); // pos, val, tag tex (slctex[sdtex])
  glUniform1fARB(m_eeParm[2], sceneRadius); // sceneRadius
  glUniform1fARB(m_eeParm[4], (m_edge-1)); // edges
  glUniform1fARB(m_eeParm[5], m_shadow); // shadows
  glUniform1fARB(m_eeParm[6], Global::gamma()); // gamma
  glUniform1fARB(m_eeParm[7], 0.9-m_roughness*0.1); // roughness
  glUniform1fARB(m_eeParm[8], m_specular); // specular

  StaticFunctions::pushOrthoView(0, 0, wd, ht);
  StaticFunctions::drawQuad(0, 0, wd, ht, 1);
  StaticFunctions::popOrthoView();
  //----------------------------

  glUseProgramObjectARB(0);

  if (m_mixTag)
    {
      glActiveTexture(GL_TEXTURE5);
      glDisable(GL_TEXTURE_1D);
    }

  glActiveTexture(GL_TEXTURE6);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);

  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE1);
  glDisable(GL_TEXTURE_2D_ARRAY);

  glActiveTexture(GL_TEXTURE2);
  glDisable(GL_TEXTURE_RECTANGLE_ARB);


  drawGeometry();
}

void
RcViewer::drawGeometry()
{
  Vec eyepos = m_viewer->camera()->position();
  GeometryObjects::crops()->draw(m_viewer, false);
  GeometryObjects::clipplanes()->draw(m_viewer, false);
  GeometryObjects::hitpoints()->draw(m_viewer, false);

  LightHandler::giLights()->draw(m_viewer, false);
  LightHandler::giLights()->postdraw(m_viewer);

  Vec pn = Vec(0,0,0);
  float pnear = 10000;
  float pfar = -10000;
  GeometryObjects::paths()->draw(m_viewer, pn, pnear, pfar, false, eyepos);

  
  QList<Vec> clipPos;
  QList<Vec> clipNormal;
  clipPos = GeometryObjects::clipplanes()->positions();
  clipNormal = GeometryObjects::clipplanes()->normals();

  float *bdata = (m_b0xform.transposed()).data();
  double xform[16];
  for(int i=0; i<16; i++) xform[i] = bdata[i];

  GeometryObjects::trisets()->predraw(m_viewer, xform);

  GeometryObjects::trisets()->draw(m_viewer,
				   eyepos,
				   pnear, pfar,
				   Vec(1,1,1), // dummy
				   true,
				   eyepos,
				   clipPos, clipNormal);
  GeometryObjects::trisets()->postdraw(m_viewer);
  GeometryObjects::captions()->draw(m_viewer);

  if (Global::drawBox())
    {
      Vec lineColor = Vec(0.9, 0.9, 0.9);
      Vec bgcolor = Global::backgroundColor();
      float bgintensity = (0.3*bgcolor.x +
			   0.5*bgcolor.y +
			   0.2*bgcolor.z);
      if (bgintensity > 0.5)
	{
	  glDisable(GL_BLEND);
	  lineColor = Vec(0,0,0);
	}

      glColor4f(lineColor.x, lineColor.y, lineColor.z, 1.0);

      glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
      Vec voxelScaling = Global::voxelScaling();
      Vec dMin = VECPRODUCT(m_dataMin, voxelScaling);
      Vec dMax = VECPRODUCT(m_dataMax, voxelScaling);
      StaticFunctions::drawEnclosingCube(dMin, dMax);
      glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

      m_boundingBox.draw();
    }
}


bool
RcViewer::getHit(const QMouseEvent *event)
{
  bool found = false;
  Vec pos;
  QPoint scr = event->pos();
  int cx = scr.x();
  int cy = scr.y();
  int sw = m_viewer->camera()->screenWidth();
  int sh = m_viewer->camera()->screenHeight();  
  GLfloat depth = 0;
  
  glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
  glReadPixels(cx, sh-1-cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

  found = false;
  if (depth > 0.0 && depth < 1.0)
    {
      pos = m_viewer->camera()->unprojectedCoordinatesOf(Vec(cx, cy, depth));
      found = true;
    }

  if (event->buttons() == Qt::RightButton) // change rotation pivot
    {
      if (found)
	{
	  m_viewer->camera()->setRevolveAroundPoint(pos);
	  QMessageBox::information(0, "", "Rotation pivot changed");
	}
      else
	{
	  m_viewer->camera()->setRevolveAroundPoint(m_viewer->sceneCenter());
	  QMessageBox::information(0, "", "Rotation pivot reset to scene center");
	}

      return false;
    }


  if (found)
      GeometryObjects::hitpoints()->add(pos);


  return found;
}

void
RcViewer::checkCrops()
{
  if (GeometryObjects::crops()->count() == 0 &&
      m_crops.count() == 0)
    return;

  bool doall = false;
  if (GeometryObjects::crops()->count() != m_crops.count())
    doall = true;
  
  if (!doall)
    {
      QList<CropObject> co;
      co = GeometryObjects::crops()->crops();
      for(int i=0; i<m_crops.count(); i++)
	{
	  if (m_crops[i] != co[i])
	    {
	      doall = true;
	      break;
	    }
	}
    }
      
  if (doall)
    {
      m_crops.clear();
      m_crops = GeometryObjects::crops()->crops();    

      createIsoRaycastShader();
    }
}

//---------------------
//---------------------
//---------------------

void
RcViewer::boundingBoxChanged()
{
  if (m_depth == 0) // we don't have a volume yet
    return;

  //----
  Vec bmin, bmax;
  m_boundingBox.bounds(bmin, bmax);
  Vec voxelScaling = Global::voxelScaling();
  bmin = VECDIVIDE(bmin, voxelScaling);
  bmax = VECDIVIDE(bmax, voxelScaling);
  int minD = bmin.z;
  int minW = bmin.y;
  int minH = bmin.x;
  int maxD = qCeil(bmax.z); // because we are getting it as float
  int maxW = qCeil(bmax.y);
  int maxH = qCeil(bmax.x);

  if (minD == m_cminD &&
      maxD == m_cmaxD &&
      minW == m_cminW &&
      maxW == m_cmaxW &&
      minH == m_cminH &&
      maxH == m_cmaxH)
    return; // no change
  //----
  
  
  generateBoxes();

  loadAllBoxesToVBO();  

  updateFilledBoxes();
}

void
RcViewer::generateBoxes()
{
  QProgressDialog progress("Updating box structure",
			   QString(),
			   0, 100,
			   0);
  progress.setMinimumDuration(0);

  m_boxSoup.clear();
  
  int faces[] = {1, 5, 7, 3,
		 0, 2, 6, 4,
		 0, 1, 3, 2,
		 7, 5, 4, 6,
		 2, 3, 7, 6,
		 1, 0, 4, 5};
	  
  Vec voxelScaling = Global::voxelScaling();


  Vec bminO, bmaxO;
  m_boundingBox.bounds(bminO, bmaxO);

  bminO = VECDIVIDE(bminO, voxelScaling);
  bmaxO = VECDIVIDE(bmaxO, voxelScaling);

  bminO = StaticFunctions::maxVec(bminO, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
  bmaxO = StaticFunctions::minVec(bmaxO, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));

  m_cminD = bminO.z;
  m_cminW = bminO.y;
  m_cminH = bminO.x;
  m_cmaxD = qCeil(bmaxO.z); // because we are getting it as float
  m_cmaxW = qCeil(bmaxO.y);
  m_cmaxH = qCeil(bmaxO.x);

  int imin = (int)bminO.x/m_boxSize;
  int jmin = (int)bminO.y/m_boxSize;
  int kmin = (int)bminO.z/m_boxSize;

  int imax = (int)bmaxO.x/m_boxSize;
  int jmax = (int)bmaxO.y/m_boxSize;
  int kmax = (int)bmaxO.z/m_boxSize;
  if (imax*m_boxSize < (int)bmaxO.x) imax++;
  if (jmax*m_boxSize < (int)bmaxO.y) jmax++;
  if (kmax*m_boxSize < (int)bmaxO.z) kmax++;
  
  for(int k=kmin; k<kmax; k++)
    {
      progress.setValue(100*(float)k/(float)kmax);
      qApp->processEvents();


      for(int j=jmin; j<jmax; j++)
	for(int i=imin; i<imax; i++)
	  {
	    int idx = k*m_wbox*m_hbox+j*m_hbox+i;
	    
	    Vec bmin, bmax;
	    bmin = Vec(qMax(i*m_boxSize, (int)bminO.x),
		       qMax(j*m_boxSize, (int)bminO.y),
		       qMax(k*m_boxSize, (int)bminO.z));
	    
	    bmax = Vec(qMin((i+1)*m_boxSize, (int)bmaxO.x),
		       qMin((j+1)*m_boxSize, (int)bmaxO.y),
		       qMin((k+1)*m_boxSize, (int)bmaxO.z));
	    
	    Vec box[8];
	    box[0] = Vec(bmin.x,bmin.y,bmin.z);
	    box[1] = Vec(bmin.x,bmin.y,bmax.z);
	    box[2] = Vec(bmin.x,bmax.y,bmin.z);
	    box[3] = Vec(bmin.x,bmax.y,bmax.z);
	    box[4] = Vec(bmax.x,bmin.y,bmin.z);
	    box[5] = Vec(bmax.x,bmin.y,bmax.z);
	    box[6] = Vec(bmax.x,bmax.y,bmin.z);
	    box[7] = Vec(bmax.x,bmax.y,bmax.z);
	    
	    float xmin, xmax, ymin, ymax, zmin, zmax;
	    xmin = (bmin.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
	    xmax = (bmax.x-m_minHSlice)/(m_maxHSlice-m_minHSlice);
	    ymin = (bmin.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
	    ymax = (bmax.y-m_minWSlice)/(m_maxWSlice-m_minWSlice);
	    zmin = (bmin.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);
	    zmax = (bmax.z-m_minDSlice)/(m_maxDSlice-m_minDSlice);
	    
	    for(int b=0; b<8; b++)
	      box[b] = VECPRODUCT(box[b], voxelScaling);
	      
	    QList<Vec> vboSoup;
	    for(int v=0; v<6; v++)
	      {
		Vec poly[4];
		for(int w=0; w<4; w++)
		  {
		    int idx = faces[4*v+w];
		    poly[w] = box[idx];
		  }
		
		// put in vboSoup
		for(int t=0; t<2; t++)
		  {
		    vboSoup << poly[0];
		    vboSoup << poly[t+1];		    
		    vboSoup << poly[t+2];
		  }
	      }
	    m_boxSoup << vboSoup;
	  } //i j
    } // k
  
  m_numBoxes = m_boxSoup.count();

  progress.setValue(100);
}

void
RcViewer::loadAllBoxesToVBO()
{
  QProgressDialog progress("Loading box structure to vbo",
			   QString(),
			   0, 100,
			   0);
  progress.setMinimumDuration(0);

  m_mdEle = 0;
  if (m_mdCount) delete [] m_mdCount;
  if (m_mdIndices) delete [] m_mdIndices;  
  m_mdCount = new GLsizei[m_numBoxes];
  m_mdIndices = new GLint[m_numBoxes];

  //---------------------
  int nvert = 0;
  for(int i=0; i<m_numBoxes; i++)
    nvert += m_boxSoup[i].count();

  m_ntri = nvert/3;
  int nv = 3*nvert;
  int ni = 3*m_ntri;
  float *vertData;
  vertData = new float[nv];
  int vi=0;
  for(int i=0; i<m_numBoxes; i++)
    {
      if ((i/100)%10 == 0)
	{
	  progress.setValue(100*(float)i/(float)m_numBoxes);
	  qApp->processEvents();
	}
      for(int b=0; b<m_boxSoup[i].count(); b++)
	{
	  vertData[3*vi+0] = m_boxSoup[i][b].x;
	  vertData[3*vi+1] = m_boxSoup[i][b].y;
	  vertData[3*vi+2] = m_boxSoup[i][b].z;
	  vi++;
	}
    }
  //---------------------


  
  unsigned int *indexData;
  indexData = new unsigned int[ni];
  for(int i=0; i<ni; i++)
    indexData[i] = i;
  //---------------------

  progress.setValue(50);
  qApp->processEvents();


  if(m_glVertArray)
    {
      glDeleteBuffers(1, &m_glIndexBuffer);
      glDeleteVertexArrays( 1, &m_glVertArray );
      glDeleteBuffers(1, &m_glVertBuffer);
      m_glIndexBuffer = 0;
      m_glVertArray = 0;
      m_glVertBuffer = 0;
    }

  glGenVertexArrays(1, &m_glVertArray);
  glBindVertexArray(m_glVertArray);
      
  // Populate a vertex buffer
  glGenBuffers(1, &m_glVertBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_glVertBuffer);
  glBufferData(GL_ARRAY_BUFFER,
	       sizeof(float)*nv,
	       vertData,
	       GL_STATIC_DRAW);

  progress.setValue(70);
  qApp->processEvents();


  // Identify the components in the vertex buffer
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			sizeof(float)*3, // stride
			(void *)0); // starting offset

  
  // Create and populate the index buffer
  glGenBuffers(1, &m_glIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
	       sizeof(unsigned int)*ni,
	       indexData,
	       GL_STATIC_DRAW);
  
  progress.setValue(90);
  qApp->processEvents();


  glBindVertexArray(0);

  delete [] vertData;
  delete [] indexData;

  m_boxSoup.clear();
  
  progress.setValue(100);
  qApp->processEvents();
}

void
RcViewer::updateFilledBoxes()
{
  int lmin = 255;
  int lmax = 0;

  int iend = 255;
  if (m_bytesPerVoxel == 2)
    iend = 65535;
  
  for(int i=0; i<iend; i++)
    {
      if (m_lut[4*i+3] > 2)
	{
	  lmin = i;
	  break;
	}
    }
  for(int i=iend; i>0; i--)
    {
      if (m_lut[4*i+3] > 2)
	{
	  lmax = i;
	  break;
	}
    }
    
  Vec bminO, bmaxO;
  m_boundingBox.bounds(bminO, bmaxO);

  Vec voxelScaling = Global::relativeVoxelScaling();
  bminO = VECDIVIDE(bminO, voxelScaling);
  bmaxO = VECDIVIDE(bmaxO, voxelScaling);

  bminO = StaticFunctions::maxVec(bminO, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
  bmaxO = StaticFunctions::minVec(bmaxO, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));

  m_filledBoxes.fill(true);

  //-------------------------------------------
  //-- identify filled boxes
  Global::progressBar()->show();
//  QProgressDialog progress("Marking valid boxes - (1/2)",
//			   QString(),
//			   0, 100,
//			   0);
//  progress.setMinimumDuration(0);


  Global::progressBar()->setValue(10);
  //progress.setValue(10);
  qApp->processEvents();

  int boxVoxPerc = qPow((m_boxSize/m_sslevel),3)*0.5; //10 percent
  
  for(int d=0; d<m_dbox; d++)
    {
      Global::progressBar()->setValue(100*d/m_dbox);
      //progress.setValue(100*d/m_dbox);
      qApp->processEvents();

      for(int w=0; w<m_wbox; w++)
	for(int h=0; h<m_hbox; h++)
	  {
	    bool ok = true;
	    
	    // consider only current bounding box	 
	    if ((d*m_boxSize < bminO.z && (d+1)*m_boxSize < bminO.z) ||
		(d*m_boxSize > bmaxO.z && (d+1)*m_boxSize > bmaxO.z) ||
		(w*m_boxSize < bminO.y && (w+1)*m_boxSize < bminO.y) ||
		(w*m_boxSize > bmaxO.y && (w+1)*m_boxSize > bmaxO.y) ||
		(h*m_boxSize < bminO.x && (h+1)*m_boxSize < bminO.x) ||
		(h*m_boxSize > bmaxO.x && (h+1)*m_boxSize > bmaxO.x))
	      ok = false;
	    
	    int idx = d*m_wbox*m_hbox+w*m_hbox+h;
	    if (ok)
	      {
		int vmin = m_boxMinMax[2*idx+0];
		int vmax = m_boxMinMax[2*idx+1];
		if ((vmin < lmin && vmax < lmin) || 
		    (vmin > lmax && vmax > lmax))
		  ok = false;

//		if (ok)
//		  {
//		    int nvo = 0;
//		    for(int hv=lmin; hv<=lmax; hv++)
//		      nvo += m_boxHistogram[idx][hv];
//		    if (nvo < boxVoxPerc)
//		      ok = false;
//		  }
	      }

	    m_filledBoxes.setBit(idx, ok);
	  }
    }
  Global::progressBar()->setValue(100);
  //progress.setValue(100);
  qApp->processEvents();


  generateDrawBoxes();
}

void
RcViewer::generateDrawBoxes()
{ 
  m_mdEle = 0;
  
  Vec voxelScaling = Global::voxelScaling();

  Vec bminO, bmaxO;
  m_boundingBox.bounds(bminO, bmaxO);

  bminO = VECDIVIDE(bminO, voxelScaling);
  bmaxO = VECDIVIDE(bmaxO, voxelScaling);

  bminO = StaticFunctions::maxVec(bminO, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
  bmaxO = StaticFunctions::minVec(bmaxO, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));

  int imin = (int)bminO.x/m_boxSize;
  int jmin = (int)bminO.y/m_boxSize;
  int kmin = (int)bminO.z/m_boxSize;

  int imax = (int)bmaxO.x/m_boxSize;
  int jmax = (int)bmaxO.y/m_boxSize;
  int kmax = (int)bmaxO.z/m_boxSize;
  if (imax*m_boxSize < (int)bmaxO.x) imax++;
  if (jmax*m_boxSize < (int)bmaxO.y) jmax++;
  if (kmax*m_boxSize < (int)bmaxO.z) kmax++;

  int mdC = 0;
  int mdI = -1;
  
  int bid = 0;
  for(int k=kmin; k<kmax; k++)
    {
      Global::progressBar()->setValue(100*k/kmax);
      qApp->processEvents();
  
      for(int j=jmin; j<jmax; j++)
	for(int i=imin; i<imax; i++)
	  {
	    int idx = k*m_wbox*m_hbox+j*m_hbox+i;
	    if (m_filledBoxes.testBit(idx))
	      {
		if (mdI < 0) mdI = bid;
		mdC++;
	      }
	    else
	      {
		if (mdI > -1)
		  {
		    // 2 triangles per face - 36 triangles in all for a cube
		    m_mdIndices[m_mdEle] = mdI*36;
		    m_mdCount[m_mdEle] = mdC*36;
		    m_mdEle++;
		    if (m_mdEle >= m_numBoxes)
		QMessageBox::information(0, "", QString("ele > %1").arg(m_numBoxes));
		    
		    mdI = -1;
		    mdC = 0;
		  }
	      }
	    bid++;
	  }
    }

  if (mdI > -1)
    {
      m_mdIndices[m_mdEle] = mdI*36;
      m_mdCount[m_mdEle] = mdC*36;
      m_mdEle++;
    }

  Global::progressBar()->setValue(100);
  qApp->processEvents();
}

void
RcViewer::drawVBOBox(GLenum glFaces)
{
  glEnable(GL_CULL_FACE);
  glCullFace(glFaces);

  glBindVertexArray(m_glVertArray);
  glBindBuffer(GL_ARRAY_BUFFER, m_glVertBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glIndexBuffer);  

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glUseProgram(RcShaderFactory::boxShader());

  
  GLint *boxShaderParm = RcShaderFactory::boxShaderParm();  


  // model-view-projection matrix
  GLdouble m[16];
  m_viewer->camera()->getModelViewProjectionMatrix(m);
  QMatrix4x4 matMVP;
  for(int i=0; i<16; i++) matMVP.data()[i] = m[i];

  
  QMatrix4x4 b0xform;
  b0xform.setToIdentity();

  Vec pivot = m_brickInfo[0].getCorrectedPivot();
  Vec axis = m_brickInfo[0].axis;
  float angle = m_brickInfo[0].angle;

  Vec bminO, bmaxO;
  m_boundingBox.bounds(bminO, bmaxO);
//  bminO = StaticFunctions::maxVec(bminO, Vec(m_minHSlice, m_minWSlice, m_minDSlice));
//  bmaxO = StaticFunctions::minVec(bmaxO, Vec(m_maxHSlice, m_maxWSlice, m_maxDSlice));

  pivot = VECPRODUCT((Vec(1,1,1)-pivot),bminO) + VECPRODUCT(pivot,bmaxO);
    
  b0xform.translate(pivot.x, pivot.y, pivot.z);
  b0xform.rotate(angle, axis.x, axis.y, axis.z);
  b0xform.translate(-pivot.x, -pivot.y, -pivot.z);

  matMVP = matMVP * b0xform ;
  

  glUniformMatrix4fv(boxShaderParm[0], 1, GL_FALSE, matMVP.data());

  glMultiDrawArrays(GL_TRIANGLES, m_mdIndices, m_mdCount, m_mdEle);  
  
  glDisableVertexAttribArray(0);
  glDisableVertexAttribArray(1);

  glBindVertexArray(0);

  glUseProgram(0);

  glDisable(GL_CULL_FACE);
}


void
RcViewer::setAMRTex(GLuint atex)
{
  m_amrTex = atex;
}

void
RcViewer::setAMR(bool m)
{
  m_amrData = m;

  createIsoRaycastShader();
}
