#include "rcshaderfactory.h"
#include "cropshaderfactory.h"
#include "blendshaderfactory.h"
#include "shaderfactory.h"
#include "global.h"

#include <QTextEdit>
#include <QVBoxLayout>
#include <QMessageBox>


bool
RcShaderFactory::loadShader(GLhandleARB &progObj,
			  QString shaderString)
{
  GLhandleARB fragObj = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);  
  glAttachObjectARB(progObj, fragObj);

  GLhandleARB vertObj = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);  
  glAttachObjectARB(progObj, vertObj);

  {  // vertObj
    QString qstr;
    qstr = "varying vec3 pointpos;\n";
    qstr += "void main(void)\n";
    qstr += "{\n";
    qstr += "  // Transform vertex position into homogenous clip-space.\n";
    qstr += "  gl_FrontColor = gl_Color;\n";
    qstr += "  gl_BackColor = gl_Color;\n";
    qstr += "  gl_Position = ftransform();\n";
    qstr += "  gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n";
    qstr += "  gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;\n";
    qstr += "  gl_TexCoord[2] = gl_TextureMatrix[2] * gl_MultiTexCoord2;\n";
    qstr += "  gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;\n";
    qstr += "  pointpos = gl_Vertex.xyz;\n";
    qstr += "}\n";
    
    int len = qstr.length();
    char *tbuffer = new char[len+1];
    sprintf(tbuffer, qstr.toLatin1().data());
    const char *sstr = tbuffer;
    glShaderSourceARB(vertObj, 1, &sstr, NULL);
    delete [] tbuffer;

    GLint compiled = -1;
    glCompileShaderARB(vertObj);
    glGetObjectParameterivARB(vertObj,
			      GL_OBJECT_COMPILE_STATUS_ARB,
			      &compiled);
    if (!compiled)
      {
	GLcharARB str[1000];
	GLsizei len;
	glGetInfoLogARB(vertObj,
			(GLsizei) 1000,
			&len,
			str);
	
	QMessageBox::information(0,
				 "Error : Vertex Shader",
				 str);
	return false;
    }
  }
    
    
  { // fragObj
    int len = shaderString.length();
    char *tbuffer = new char[len+1];
    sprintf(tbuffer, shaderString.toLatin1().data());
    const char *sstr = tbuffer;
    glShaderSourceARB(fragObj, 1, &sstr, NULL);
    delete [] tbuffer;
  
    GLint compiled = -1;
    glCompileShaderARB(fragObj);
    glGetObjectParameterivARB(fragObj,
			    GL_OBJECT_COMPILE_STATUS_ARB,
			      &compiled);
    if (!compiled)
      {
	GLcharARB str[1000];
	GLsizei len;
	glGetInfoLogARB(fragObj,
			(GLsizei) 1000,
			&len,
			str);
	
	//-----------------------------------
	// display error
	
	//qApp->beep();
	
	QString estr;
	QStringList slist = shaderString.split("\n");
	for(int i=0; i<slist.count(); i++)
	  estr += QString("%1 : %2\n").arg(i+1).arg(slist[i]);
	
	QTextEdit *tedit = new QTextEdit();
	tedit->insertPlainText("-------------Error----------------\n\n");
	tedit->insertPlainText(str);
	tedit->insertPlainText("\n-----------Shader---------------\n\n");
	tedit->insertPlainText(estr);
	
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(tedit);
	
	QDialog *showError = new QDialog();
	showError->setWindowTitle("Error in Fragment Shader");
	showError->setSizeGripEnabled(true);
	showError->setModal(true);
	showError->setLayout(layout);
	showError->exec();
	//-----------------------------------
	
	return false;
      }
  }

  
  //----------- link program shader ----------------------
  GLint linked = -1;
  glLinkProgramARB(progObj);
  glGetObjectParameterivARB(progObj, GL_OBJECT_LINK_STATUS_ARB, &linked);
  if (!linked)
    {
      GLcharARB str[1000];
      GLsizei len;
      QMessageBox::information(0,
			       "ProgObj",
			       "error linking texProgObj");
      glGetInfoLogARB(progObj,
		      (GLsizei) 1000,
		      &len,
		      str);
      QMessageBox::information(0,
			       "Error",
			       QString("%1\n%2").arg(len).arg(str));
      return false;
    }

  glDeleteObjectARB(fragObj);
  glDeleteObjectARB(vertObj);

  return true;
}

QString
RcShaderFactory::genRectBlurShaderString(int filter)
{
  QString shader;

  shader = "#extension GL_ARB_texture_rectangle : enable\n";
  shader += "uniform sampler2DRect blurTex;\n";
  shader += "uniform float minZ;\n";
  shader += "uniform float maxZ;\n";
  shader += "\n";
  shader += "void main(void)\n";
  shader += "{\n";
  shader += "  vec4 color;\n";
  shader += "  vec2 spos = gl_FragCoord.xy;\n";
  shader += "\n";
  
  if (filter == 1) // bilateral filter
    {
      shader += "  color = texture2DRect(blurTex, spos.xy);\n";
      shader += "  gl_FragColor = color;\n";
      shader += "  if (color.a < 0.01) return;\n";

      float cx[8] = {-1.0,-1.0,-1.0, 0.0, 0.0, 1.0, 1.0, 1.0};
      float cy[8] = {-1.0, 0.0, 1.0,-1.0, 1.0,-1.0, 0.0, 1.0};
      shader += "  float cx[8];\n";
      shader += "  float cy[8];\n";
      for(int i=0; i<8; i++)
	shader += QString("  cx[%1] = float(%2);\n").arg(i).arg(cx[i]);
      for(int i=0; i<8; i++)
	shader += QString("  cy[%1] = float(%2);\n").arg(i).arg(cy[i]);

      shader += "  float depth = color.x;\n";
      shader += "  float odepth = minZ + depth*(maxZ-minZ);\n";
      shader += "  float sum = 1.0;\n";
      shader += "  for(int i=0; i<8; i++)\n";
      shader += "  {\n";
      shader += "    float z = texture2DRect(blurTex, spos.xy + vec2(cx[i],cy[i])).x;\n";
      shader += "    float oz = minZ + z*(maxZ-minZ);\n";
      shader += "    float fi = (odepth-oz)*0.2;\n";
      shader += "    fi = exp(-fi*fi);\n";
      shader += "    depth += z*fi;\n";
      shader += "    sum += fi;\n";
      shader += "  }\n";

      shader += "  gl_FragColor.rgba = vec4(depth/sum, color.gb, color.a);\n";
    }
  else if (filter == 2)
    { // gaussian filter
      shader += "  color  = vec4(4.0,4.0,4.0,4.0)*texture2DRect(blurTex, spos.xy);\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2( 0.0, 1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2( 0.0,-1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2( 1.0, 0.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2(-1.0, 0.0));\n";
      shader += "  color += texture2DRect(blurTex, spos.xy + vec2( 1.0, 1.0));\n";
      shader += "  color += texture2DRect(blurTex, spos.xy + vec2( 1.0,-1.0));\n";
      shader += "  color += texture2DRect(blurTex, spos.xy + vec2(-1.0, 1.0));\n";
      shader += "  color += texture2DRect(blurTex, spos.xy + vec2(-1.0,-1.0));\n";
      shader += "  gl_FragColor.rgba = color/16.0;\n";
    }
  else if (filter == 3)
    { // sharpness filter
      shader += "  color = vec4(8.0,8.0,8.0,8.0)*texture2DRect(blurTex, spos.xy);\n";

      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2(1.0,0.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy - vec2(1.0,0.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2(0.0,1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy - vec2(0.0,1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2( 1.0, 1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2(-1.0, 1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2( 1.0,-1.0));\n";
      shader += "  color += vec4(2.0,2.0,2.0,2.0)*texture2DRect(blurTex, spos.xy + vec2(-1.0,-1.0));\n";

      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 2.0, 2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 2.0, 1.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 2.0, 0.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 2.0,-1.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 2.0,-2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-2.0, 2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-2.0, 1.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-2.0, 0.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-2.0,-1.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-2.0,-2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-1.0, 2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 0.0, 2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 1.0, 2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2(-1.0,-2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 0.0,-2.0));\n";
      shader += "  color -= texture2DRect(blurTex, spos.xy + vec2( 1.0,-2.0));\n";

      shader += "  gl_FragColor.rgba = color/8.0;\n";
    }
  else
    shader += "  color = texture2DRect(blurTex, spos.xy);\n";

  shader += "  gl_FragColor = clamp(gl_FragColor, vec4(0.0,0.0,0.0,0.0), vec4(1.0,1.0,1.0,1.0));\n";

  shader += "}\n";

  return shader;
}

QString
RcShaderFactory::gradMagnitude()
{
  QString shader;

//  shader += "float gradMagnitude(vec3 voxelCoord, vec3 onev)\n";
//  shader += "{\n";
//  shader += "  float dx = (texture3D(dataTex, voxelCoord+vec3(onev.x,0,0)).x-\n";
//  shader += "	           texture3D(dataTex, voxelCoord-vec3(onev.x,0,0)).x);\n";
//  shader += "  float dy = (texture3D(dataTex, voxelCoord+vec3(0,onev.y,0)).x-\n";
//  shader += "	           texture3D(dataTex, voxelCoord-vec3(0,onev.y,0)).x);\n";
//  shader += "  float dz = (texture3D(dataTex, voxelCoord+vec3(0,0,onev.z)).x-\n";
//  shader += "	           texture3D(dataTex, voxelCoord-vec3(0,0,onev.z)).x);\n";
//  shader += "\n";
//  shader += "  return sqrt(dx*dx + dy*dy + dz*dz);\n";
//  shader += "}\n";

  // using tetrahedron technique - iquilezles.org (normalsSDF) - Paul Malin in ShaderToy
  //shader += "float gradMagnitude(vec3 voxelCoord, vec3 onev, float maxLayers)\n";
  shader += "vec3 gradMagnitude(vec3 voxelCoord, vec3 onev, float maxLayers)\n";
  shader += "{\n";
  shader += "  vec2 k = vec2(1.0, -1.0);\n";
//  shader += "  vec3 grad = (k.xyy * texture(dataTex, voxelCoord+k.xyy*onev).x +\n";
//  shader += "               k.yyx * texture(dataTex, voxelCoord+k.yyx*onev).x +\n";
//  shader += "               k.yxy * texture(dataTex, voxelCoord+k.yxy*onev).x +\n";
//  shader += "               k.xxx * texture(dataTex, voxelCoord+k.xxx*onev).x);\n";
  shader += "  vec3 grad = (k.xyy * getVal(voxelCoord+k.xyy*onev, maxLayers, 1.0) +\n";
  shader += "               k.yyx * getVal(voxelCoord+k.yyx*onev, maxLayers, 1.0) +\n";
  shader += "               k.yxy * getVal(voxelCoord+k.yxy*onev, maxLayers, 1.0) +\n";
  shader += "               k.xxx * getVal(voxelCoord+k.xxx*onev, maxLayers, 1.0));\n";
  shader += "  grad = grad/2.0;\n";  // should be actually divided by 4
  shader += "  return grad;\n";
  //shader += "  return length(grad);\n";
  shader += "}\n";

  return shader;
}

QString
RcShaderFactory::getGrad()
{
  QString shader;

  shader += " vec3 gx, gy, gz;\n";
  shader += " gx = vec3(1.0/vsize.x,0,0);\n";
  shader += " gy = vec3(0,1.0/vsize.y,0);\n";
  shader += " gz = vec3(0,0,1.0/vsize.z);\n";
  shader += " float vx = texture3D(dataTex, voxelCoord+gx).x - texture3D(dataTex, voxelCoord-gx).x;\n";
  shader += " float vy = texture3D(dataTex, voxelCoord+gy).x - texture3D(dataTex, voxelCoord-gy).x;\n";
  shader += " float vz = texture3D(dataTex, voxelCoord+gz).x - texture3D(dataTex, voxelCoord-gz).x;\n";
  shader += " grad = vec3(vx, vy, vz);\n";

  return shader;
}

QString
RcShaderFactory::addLighting()
{
  QString shader;

  shader += " if (length(grad) > 0.1)\n";
  shader += "  {\n";
  shader += "    grad = normalize(grad);\n";
  shader += "    vec3 lightVec = viewDir;\n";
  shader += "    float diff = pow(abs(dot(lightVec, grad)),0.5);\n";
  shader += "    vec3 reflecvec = reflect(lightVec, grad);\n";
  shader += "    float spec = pow(abs(dot(grad, reflecvec)), 512.0);\n";
  shader += "   colorSample.rgb *= dot(lightparm, vec3(1.0,diff,spec));\n";
  shader += "    if (any(greaterThan(colorSample.rgb,vec3(1.0,1.0,1.0)))) \n";
  shader += "      colorSample.rgb = vec3(1.0,1.0,1.0);\n";
  shader += "  }\n";

  return shader;
}


//------------------------------
// shaders from drishti paint
//------------------------------
QString
RcShaderFactory::clip()
{
  QString shader;

  shader += "vec3 clip(vec3 pt0, vec3 dir, vec3 vcorner, vec3 vsize, vec3 voxelScale)\n";
  shader += "{\n";
  shader += " vec3 pt = pt0;\n";
  shader += " vec3 nomod = vsize/max(vsize.x, max(vsize.y, vsize.z));\n";
  shader += " if (nclip > 0)\n";
  shader += "  {\n";
  shader += "    for(int c=0; c<nclip; c++)\n";
  shader += "      {\n";
  shader += "        vec3 cpos = clipPos[c];\n";
  shader += "        vec3 cnorm = clipNormal[c];\n";
  shader += "        cpos = (cpos-vcorner)/vsize;\n"; 
  shader += "        cpos /= voxelScale;\n"; 
  shader += "        cnorm *= nomod;\n";
  shader += "        cnorm *= voxelScale;\n"; 
  shader += "        float deno = dot(dir, cnorm);\n";
  shader += "        if (deno > 0.0)\n";
  shader += "          {\n";
  shader += "            float t = -dot((pt-cpos),cnorm)/deno;\n";
  shader += "            if (t >= 0)\n";
  shader += "             {\n";
  shader += "               pt = pt + t*dir;\n";
  shader += "             }\n";
  shader += "          }\n";
  shader += "      }\n";
  shader += "  }\n";
  shader += " return pt;\n";
  shader += "}\n";

  return shader;
}

QString
RcShaderFactory::getExactVoxelCoord()
{
  QString shader;
  shader += "vec3 exactVoxelCoordinate(vec3 voxelCoord, vec3 vsize)\n";
  shader += "{\n";
  shader += "  vec3 vC = voxelCoord*vsize;\n";
  shader += "  bvec3 vclt = lessThan(floor(vC+0.5), vC);\n";
  shader += "  vC += vec3(vclt)*vec3(0.5);\n";
  shader += "  vC -= vec3(not(vclt))*vec3(0.5);\n";
  shader += "  vC /= vsize;\n";
  shader += "  return vC;\n";
  shader += "}\n";  

  return shader;
}


QString
RcShaderFactory::getVal()
{
  QString shader;
  shader += "float getVal(vec3 voxelCoord, float maxLayers, float interp)\n";
  shader += "{\n";
  shader += "  float layer = voxelCoord.z*maxLayers;\n";
  shader += "  float layer0 = min(vsize.z-1.0,floor(voxelCoord.z*maxLayers));\n";
  shader += "  float layer1 = layer0+1.0;\n";
  shader += "  float frc = layer-layer0;\n";
  shader += "  vec3 vcrd0 = vec3(voxelCoord.xy, layer0);\n";
  shader += "  vec3 vcrd1 = vec3(voxelCoord.xy, layer1);\n";
  shader += "  float val0 = texture(dataTex, vcrd0).x;\n";
  shader += "  float val1 = texture(dataTex, vcrd1).x;\n";
  shader += "  float val = mix(val0, mix(val0, val1, frc), interp);\n"; // nearest or interpolated

  shader += "  return val;\n";
  shader += "}\n";  

  return shader;
}

QString
RcShaderFactory::genRaycastShader(QList<CropObject> crops,
				  bool bit16, bool amrData)
{
  //------------------------------------
  bool cropPresent = false;
  bool tearPresent = false;
  bool viewPresent = false;
  bool glowPresent = false;
  for(int i=0; i<crops.count(); i++)
    if (crops[i].cropType() < CropObject::Tear_Tear)
      cropPresent = true;
    else if (crops[i].cropType() < CropObject::View_Tear)
      tearPresent = true;
    else if (crops[i].cropType() < CropObject::Glow_Ball)
      viewPresent = true;
    else
      glowPresent = true;

  for(int i=0; i<crops.count(); i++)
    if (crops[i].cropType() >= CropObject::View_Tear &&
	crops[i].cropType() <= CropObject::View_Block &&
	crops[i].magnify() > 1.0)
      tearPresent = true;
  //------------------------------------

  QString shader;
  shader += "#version 450 core\n";
  //shader += "uniform sampler3D dataTex;\n";
  shader += "uniform sampler2DArray dataTex;\n";
  shader += "uniform sampler2D lutTex;\n";
  shader += "uniform sampler2DRect exitTex;\n";
  shader += "uniform sampler2DRect entryTex;\n";
  shader += "uniform sampler1D tagTex;\n";
  shader += "uniform float stepSize;\n";
  shader += "uniform vec3 eyepos;\n";
  shader += "uniform vec3 vcorner;\n";
  shader += "uniform vec3 vsize;\n";
  shader += "uniform vec3 voxelScale;\n";
  shader += "uniform bool saveCoord;\n";
  shader += "uniform int skipLayers;\n";
  shader += "uniform int skipVoxels;\n";
  shader += "uniform bool mixTag;\n";
  shader += "uniform int nclip;\n";
  shader += "uniform vec3 clipPos[5];\n";
  shader += "uniform vec3 clipNormal[5];\n";
  shader += "uniform float maxDeep;\n";
  shader += "uniform float minGrad;\n";
  shader += "uniform float maxGrad;\n";
  shader += "uniform float sslevel;\n";
  shader += "uniform mat4 MVP;\n";
  shader += "uniform float interp;\n";

  shader += "uniform sampler2DRect lightTex;\n";
  shader += "uniform int lightgridx;\n";
  shader += "uniform int lightgridy;\n";
  shader += "uniform int lightgridz;\n";
  shader += "uniform int lightnrows;\n";
  shader += "uniform int lightncols;\n";
  shader += "uniform int lightlod;\n";

  shader += "uniform float gamma;\n";

  if (amrData)
    shader += "uniform sampler2DRect amrTex;\n";

  shader += "uniform float ambient;\n";
  shader += "uniform float diffuse;\n";
  shader += "uniform float roughness;\n";
  shader += "uniform float specular;\n";

  shader += "uniform vec3 viewDir;\n";

  
  shader += "layout(location = 0) out vec4 outColor;\n";
  shader += "layout(location = 1) out vec4 outDepth;\n";
  
  shader += ShaderFactory::ggxShader();

  if (cropPresent) shader += CropShaderFactory::generateCropping(crops);
  if (viewPresent) shader += BlendShaderFactory::generateBlend(crops);

  shader += ShaderFactory::genTextureCoordinate();
  
  //---------------------
  // get voxel value from array texture
  shader += getVal();
  //---------------------

  //---------------------
  // apply clip planes to modify entry and exit points
  shader += clip();
  //---------------------

  //---------------------
  // gradient and gradient magnitude evaluation
  //shader += getGrad();
  //---------------------

  //---------------------
  // gradient and gradient magnitude evaluation
  shader += gradMagnitude();
  //---------------------

  //---------------------
  // voxel coordinate for saving coordinate or near neighbour interpolation
  shader += getExactVoxelCoord();
  //---------------------

  float lastSet = (Global::lutSize()-1.0)/Global::lutSize();

  shader += "void main(void)\n";
  shader += "{\n";

  shader += QString("float lastSet = float(%1);\n").arg(lastSet);

  shader += "vec4 exP = texture(exitTex, gl_FragCoord.st);\n";
  shader += "vec4 enP = texture(entryTex, gl_FragCoord.st);\n";

  shader += "outColor = vec4(0.0);\n";
  shader += "outDepth = vec4(0.0);\n";

  shader += "if (exP.a < 0.001 || enP.a < 0.001) return;\n";

  shader += "vec3 exitPoint = exP.rgb;\n";
  shader += "vec3 entryPoint = enP.rgb;\n";

  //==========
  shader += "entryPoint =  entryPoint/voxelScale;\n";
  shader += "entryPoint = (entryPoint - vcorner)/vsize;\n";
  shader += "exitPoint =  exitPoint/voxelScale;\n";
  shader += "exitPoint = (exitPoint - vcorner)/vsize;\n";
  //==========
  
  shader += "vec3 onev;\n";
  shader += "onev = vec3(sslevel)/vsize;\n";
  
  shader += "vec3 dir = (exitPoint-entryPoint);\n";

  shader += "entryPoint= clip(entryPoint, dir, vcorner, vsize, voxelScale);\n";
  shader += "exitPoint = clip(exitPoint, -dir, vcorner, vsize, voxelScale);\n";

  
  shader += "vec3 dirN = (exitPoint-entryPoint);\n";
  shader += "if (dot(dir, dirN) <= 0) return;\n";
  shader += "dir = dirN;\n";

  shader += "float totlen = length(dir);\n";
  shader += "if (totlen < 0.001) return;\n";

  //shader += "float checkGrad = (minGrad > 0.0 || maxGrad < 1.0) ? 1.0 : 0.0;\n";
  shader += "float checkGrad = 1.0;\n";

  shader += "vec3 deltaDir = normalize(dir)*stepSize;\n";
  shader += "float deltaDirLen = length(deltaDir);\n";

  shader += "vec3 voxelCoord = entryPoint;\n";
  shader += "vec4 colorAcum = vec4(0.0);\n"; // The dest color
  shader += "float lengthAcum = 0.0;\n";

  //----------------
  // modify coordinate so that it aligns well with respect to eye
  shader += "{\n";
  shader += " vec3 voxpos = vcorner + voxelCoord*vsize;\n";
  shader += " vec3 I = voxpos - eyepos;\n";
  shader += " float z = length(I)/stepSize;\n";
  shader += " I = normalize(I)*stepSize;\n";
  shader += " voxelCoord = (eyepos + floor(z)*I - vcorner)/vsize;\n";
  shader += "}\n";
  //----------------
  
  shader += "float gotFirstHit = 0;\n";
  shader += "vec3 skipVoxStart = vec3(0.0);\n";

  shader += "int iend = int(length(exitPoint-entryPoint)/stepSize);\n";
  
  shader += "float maxLayers = vsize.z/sslevel;\n";

  
  //-------------------------
  // get the first hit
  //-------------------------
  shader += "int istart = 0;\n";
  shader += "for(int i=0; i<iend; i++)\n";
  shader += "{\n";
  shader += "  istart = i;\n";

  if (amrData)
    {
      shader += "  float amrX = texture2DRect(amrTex, vec2(voxelCoord.x*vsize.x,0)).x;\n";
      shader += "  float amrY = texture2DRect(amrTex, vec2(voxelCoord.y*vsize.y,0)).y;\n";
      shader += "  float amrZ = texture2DRect(amrTex, vec2(voxelCoord.z*vsize.z,0)).z;\n";
      
      shader += "  vec3 amrCoord = vec3(amrX, amrY, amrZ)/vsize;\n";

      shader += "  float val = getVal(amrCoord, maxLayers, interp);\n";
    }
  else
    shader += "  float val = getVal(voxelCoord, maxLayers, interp);\n";

  
  //shader += "  float val = getVal(voxelCoord, maxLayers, interp);\n";
  shader += "  vec4 colorSample = vec4(0.0);\n";

  if (!bit16)
    {
      shader += "  colorSample = texture(lutTex, vec2(val,0.0));\n";
      shader += "  colorSample.rgb += texture(lutTex, vec2(val,lastSet)).rgb;\n";
    }
  else
    {
      shader += "  int h0 = int(65535.0*val);\n";
      shader += "  int h1 = h0 / 256;\n";
      shader += "  h0 = int(mod(float(h0),256.0));\n";
      shader += "  float fh0 = float(h0)/256.0;\n";
      shader += "  float fh1 = float(h1)/256.0;\n";
      shader += QString("  fh1 /= float(%1);\n").arg(Global::lutSize());
      shader += "  colorSample = texture(lutTex, vec2(fh0,fh1));\n";
      shader += "  colorSample.rgb += texture(lutTex, vec2(fh0, fh1+lastSet)).rgb;\n";
    }

  // find gradient magnitude
  shader += "  if (checkGrad > 0 && colorSample.a > 0.001)\n";  
  shader += "    {\n";
  //shader += "      float gradMag = gradMagnitude(voxelCoord, onev, maxLayers);\n";
  shader += "      float gradMag = length(gradMagnitude(voxelCoord, onev, maxLayers));\n";
  shader += "      colorSample = mix(vec4(0.0), colorSample,\n";
  shader += "                        step(minGrad, gradMag)*step(gradMag, maxGrad));\n";
  shader += "    }\n";

  // reduce opacity
  shader += " colorSample.a *= colorSample.a;\n";


  if (cropPresent)
    {
      shader += "  float feather = 0.0;\n";
      shader += "  vec3 vC = voxelCoord*vsize;\n";
      shader += "  vec3 texCoord = vC*vec3(sslevel)+vcorner;\n";
      shader += "  feather = crop(texCoord, false);\n";
      shader += "  if (feather > 0.5)\n";
      shader += "    colorSample.a = 0.0;\n"; // cropped
    }
  
  // --- handle tags ---
  shader += "    if (mixTag)\n";
  shader += "    {\n";
  if (!bit16)
    shader += "      vec4 tagcolor = texture(tagTex, val);\n";
  else
    shader += "      vec4 tagcolor = texture(tagTex, fh0);\n";
  shader += "      if (tagcolor.a > 0.0)\n";
  shader += "        colorSample.rgb = mix(colorSample.rgb, tagcolor.rgb, tagcolor.a);\n";
  shader += "      else\n";
  shader += "        colorSample = vec4(0.0);\n";
  shader += "    }\n";
  // --------------------
  
  
  shader += "  if (gotFirstHit < 1.0 && colorSample.a > 0.001)\n";  
  shader += "  {\n";
  // use bisection method to get a more finer result
  shader += "    vec3 dD = -deltaDir*0.5;\n";
  shader += "    for (int b=0; b<5; b++)\n";
  shader += "    {\n";
  shader += "      voxelCoord += dD;\n";
  //shader += "      val = texture(dataTex, voxelCoord).x;\n";
  shader += "      val = getVal(voxelCoord, maxLayers, interp);\n";
  shader += "      vec4 cS = texture(lutTex, vec2(val,0.0));\n";
  shader += "      dD = dD*0.5;\n";
  shader += "      dD *= mix(-1.0, 1.0, step(0.001, cS.a));\n";
  shader += "      colorSample = mix(colorSample, cS, step(0.001, cS.a));\n";
  shader += "    }\n";
  shader += "    gotFirstHit = 1.0;\n";
  shader += "    skipVoxStart = voxelCoord*vsize;\n";
  shader += "    i = iend;\n";
  shader += "    if (saveCoord)";
  shader += "      {\n";
  shader += "        vec3 vC = exactVoxelCoordinate(voxelCoord, vsize);\n";
  shader += "        outColor = vec4(vC,1.0);\n";
  shader += "        return;\n";
  shader += "      }\n";
  shader += "  }\n";
  shader += "  else\n";
  shader += "  {\n";
  shader += "    voxelCoord += deltaDir;\n";
  shader += "    lengthAcum += deltaDirLen;\n";
  shader += "  }\n";

  shader += "}\n";  // wait to get the first hit
  //-------------------------
  //-------------------------
  
  //-------------------------
  // we have now got the first hit
  //-------------------------
//  // change max ray depth based on screen position
//  shader += "float fcx = (gl_FragCoord.x-swidth*0.5);\n";
//  shader += "float fcy = (gl_FragCoord.y-sheight*0.5);\n";
//  shader += "float sigma = max(swidth*0.2, sheight*0.2);\n";
//  shader += "float fcd = exp(-(fcx*fcx + fcy*fcy)/(2.0*sigma*sigma));\n";
//  shader += "float maxDepth = maxDeep*fcd;\n";

//  shader += "float maxDepth = maxDeep;\n";
  shader += "float maxDepth = iend*maxDeep;\n";

  shader += "int nskipped = 0;\n"; 
  shader += "float solid = 0;\n";
  shader += "float deep = 0.0;\n";
  shader += "for(int i=istart; i<iend; i++)\n";
  shader += "{\n";
  //shader += "  float val = texture(dataTex, voxelCoord).x;\n";
  shader += "  float val = getVal(voxelCoord, maxLayers, interp);\n";
  shader += "  vec4 colorSample = vec4(0.0);\n";

  shader += "vec2 emisCoord;\n";
  if (!bit16)
    {
      shader += "  colorSample = texture(lutTex, vec2(val,0.0));\n";
      shader += "  emisCoord = vec2(val, lastSet);\n";
    }
  else
    {
      shader += "  int h0 = int(65535.0*val);\n";
      shader += "  int h1 = h0 / 256;\n";
      shader += "  h0 = int(mod(float(h0),256.0));\n";
      shader += "  float fh0 = float(h0)/256.0;\n";
      shader += "  float fh1 = float(h1)/256.0;\n";
      shader += QString("  fh1 /= float(%1);\n").arg(Global::lutSize());
      shader += "  colorSample = texture(lutTex, vec2(fh0,fh1));\n";
//shader += QString("  colorSample = texture(lutTex, vec2(fh0,fh1/float(%1)));\n").	\
//	        arg(Global::lutSize());
      shader += "  emisCoord = vec2(fh0, fh1+lastSet);\n";
    }

  //------------------------------------
  //------------------------------------
  // light
  shader += "vec3 lightcol = vec3(1.0,1.0,1.0);\n";
  shader += "if (lightlod > 0)\n";
  shader += "  {\n"; // calculate light color
  shader += "    float zoffset = 0.0;\n";
  shader += "    vec3 vC = voxelCoord*vsize;\n";
  shader += "    vec3 texCoord = vC*vec3(sslevel)+vcorner;\n";
  shader += "    texCoord.xy = vec2(lightgridx,lightgridy)*voxelCoord.xy;\n";
  shader += "    texCoord.z = vC.z;\n";
  shader += "    int lbZslc = int((zoffset+texCoord.z)/lightlod);\n";
  shader += "    float lbZslcf = fract((zoffset+texCoord.z)/lightlod);\n";
  shader += "    vec2 pvg0 = getTextureCoordinate(lbZslc, ";
  shader += "                  lightncols, lightgridx, lightgridy, texCoord.xy);\n";
  shader += "    vec2 pvg1 = getTextureCoordinate(lbZslc+1, ";
  shader += "                  lightncols, lightgridx, lightgridy, texCoord.xy);\n";	       
  shader += "    vec3 lc0 = texture2DRect(lightTex, pvg0).xyz;\n";
  shader += "    vec3 lc1 = texture2DRect(lightTex, pvg1).xyz;\n";
  shader += "    lightcol = mix(lc0, lc1, lbZslcf);\n";
  shader += "    lightcol = 1.0-pow((vec3(1,1,1)-lightcol),vec3(sslevel));\n";
  shader += "  }\n";





//  //------------------------------------
//  //------------------------------------
//  // racycast shadows
//  shader += "float shadow = 0.0;\n";
//  shader += "vec3 shadowVox = voxelCoord;\n";
//  //shader += "vec3 lightVec = normalize(eyepos - shadowVox)*stepSize;\n";
//  shader += "vec3 lightVec = 5*viewDir*stepSize;\n";
//  shader += "for (int shd=0; shd<10; shd++)\n";
//  shader += "{\n";
//  shader += "  shadowVox += lightVec;\n";
//  shader += "  if (any(lessThan(shadowVox, vec3(0.0))) || any(greaterThan(shadowVox, vec3(1.0))))\n";
//  shader += "    break;\n";
//  shader += "  float val = getVal(shadowVox, maxLayers, interp);\n";   
//  shader += "  vec4 c = texture(lutTex, vec2(val,0.0));\n";
//  shader += "  shadow += (1.0-shadow)*c.a;\n";
//  shader += "}\n";
//  shader += "lightcol *= max(0.2, (1.0-shadow));\n";
//  //------------------------------------
//  //------------------------------------

  




  // gamma affects light
  shader += "  lightcol = pow(lightcol, vec3(1.0/gamma));\n";

  shader += "  colorSample.rgb *= lightcol;\n";
  //------------------------------------
  //------------------------------------

  
  //------------------------------------
  // emissive
  shader += "  vec4 emisColor = texture(lutTex, emisCoord);\n";
  shader += "  emisColor.rgb *= emisColor.a;\n";
  shader += "  colorSample.rgb += emisColor.rgb;\n";
  //------------------------------------


  // find gradient magnitude
  //shader += "  if (checkGrad > 0 && colorSample.a > 0.001)\n";  
  shader += "  vec3 normal = gradMagnitude(voxelCoord, onev, maxLayers);\n";
  shader += "  float gradMag = length(normal);\n";
  shader += "  if (colorSample.a > 0.001)\n";  
  shader += "    {\n";
  //shader += "      float gradMag = gradMagnitude(voxelCoord, onev, maxLayers);\n";
  shader += "      colorSample = mix(vec4(0.0), colorSample,\n";
  shader += "                        step(minGrad, gradMag)*step(gradMag, maxGrad));\n";
  shader += "    }\n";

  // --- handle tags ---
  shader += "    if (mixTag)\n";
  shader += "    {\n";
  if (!bit16)
    shader += "      vec4 tagcolor = texture(tagTex, val);\n";
  else
    shader += "      vec4 tagcolor = texture(tagTex, fh0);\n";
  shader += "      if (tagcolor.a > 0.0)\n";
  shader += "        colorSample.rgb = mix(colorSample.rgb, tagcolor.rgb, tagcolor.a);\n";
  shader += "      else\n";
  shader += "        colorSample = vec4(0.0);\n";
  shader += "    }\n";
  // --------------------
  
  // reduce opacity
  shader += " colorSample.a *= colorSample.a;\n";
  
  // ----------------------------
  shader += "  colorSample *= step(float(skipVoxels), length(voxelCoord*vsize-skipVoxStart));\n";
  // ----------------------------

  
  if (cropPresent)
    {
      shader += "  float feather = 0.0;\n";
      shader += "  vec3 vC = voxelCoord*vsize;\n";
      shader += "  vec3 texCoord = vC*vec3(sslevel)+vcorner;\n";
      shader += "  feather = crop(texCoord, false);\n";
      shader += "  if (feather > 0.5)\n";
      shader += "    colorSample.a = 0.0;\n"; // cropped
    }

  shader += "  if (nskipped > skipLayers)\n";
  shader += "  {\n";
  shader += "  if (colorSample.a > 0.001 || deep > 0.0)\n";
  shader += "    {\n";  
  shader += "      colorSample.rgb *= colorSample.a;\n";


  //------------------------------------

  //------------------------------------
  shader += " {\n";
  shader += "  normal = normalize(normal);\n";
  shader += "  vec3 lightvec = voxelCoord - eyepos;\n";
  shader += "  lightvec = normalize(lightvec);\n";
  shader += "  vec3 reflecvec = reflect(lightvec, normal);\n";
  shader += "  float DiffMag = pow(abs(dot(normal, lightvec)), 0.5);\n";
  shader += "  vec3 Diff = (diffuse*DiffMag)*colorSample.rgb;\n";
  shader += "  vec3 Amb = ambient*colorSample.rgb;\n";
  shader += "  float litfrac = smoothstep(0.05, 0.1, gradMag);\n";
  shader += "  colorSample.rgb = mix(colorSample.rgb, Amb, litfrac);\n";
  shader += "  if (litfrac > 0.0)\n";
  shader += "   {\n";
  shader += "     vec3 frgb = colorSample.rgb + litfrac*Diff;\n";
  shader += "     if (any(greaterThan(frgb,vec3(1.0,1.0,1.0)))) \n";
  shader += "        frgb = vec3(1.0,1.0,1.0);\n";
  shader += "     if (any(greaterThan(frgb,colorSample.aaa))) \n";  
  shader += "        frgb = colorSample.aaa;\n";
  shader += "     colorSample.rgb = frgb;\n";

//  shader += "     vec3 spec = shadingSpecularGGX(normal, lightvec, lightvec, roughness*0.2, colorSample.rgb);\n";
//  shader += "     colorSample.rgb += 0.5*specular*spec;\n";
//  shader += "     colorSample = clamp(colorSample, vec4(0.0,0.0,0.0,0.0), vec4(1.0,1.0,1.0,1.0));\n";
  shader += "   }\n";
  shader += " }\n";
  //------------------------------------



  //------------------------------------

  shader += "      colorSample.rgb = pow(colorSample.rgb, vec3(gamma));\n";
  shader += "      colorAcum += (1.0 - colorAcum.a) * colorSample;\n";
  shader += "      deep = deep + 1.0;\n";
  shader += "      if (deep < 1.1)\n"; // first hit
  shader += "       {\n";
  shader += "         vec3 voxpos = (vcorner + voxelCoord*vsize)*voxelScale;";
  shader += "         vec4 scrpos = MVP * vec4(voxpos, 1);\n";
  shader += "         float z = ((gl_DepthRange.diff * scrpos.z/scrpos.w) + \n";
  shader += "                     gl_DepthRange.near + gl_DepthRange.far) / 2.0;\n";
  shader += "         gl_FragDepth = z;\n";
  shader += "         float zLinear = length(vcorner+voxelCoord*vsize - eyepos);\n";
  shader += "         outDepth = vec4(z,val,zLinear,1.0);\n";
  shader += "       }\n";
  shader += "      if (colorAcum.a > 0.95 || deep >= maxDepth)\n";
  shader += "       {\n";
  shader += "         outColor = colorAcum;\n";
  shader += "         return;\n";
  shader += "       }\n";
  shader += "    }\n"; // colorsample || deep
  shader += "  }\n"; // gotfirsthit && nskipped > skipLayers

  shader += "  if (lengthAcum >= totlen )\n";
  shader += "      break;\n";  // terminate if opacity > 1 or the ray is outside the volume	

  shader += "  voxelCoord += deltaDir;\n";
  shader += "  lengthAcum += deltaDirLen;\n";

  shader += "  if (colorSample.a > 0.001)\n";
  shader += "   {\n";  
  shader += "      if (solid < 1.0)\n";
  shader += "        {\n";    
  shader += "          solid = 1.0;\n";
  shader += "          nskipped++;\n";  
  shader += "        }\n";  
  shader += "   }\n";
  shader += "  else\n";
  shader += "   {\n";  
  shader += "      if (solid > 0.0)\n";
  shader += "        {\n";    
  shader += "          solid = 0.0;\n";
  shader += "        }\n";  
  shader += "   }\n";

  shader += "}\n";

  //shader += "if (colorAcum.a > 0.001) outColor = colorAcum/colorAcum.a;\n";
  shader += "  colorAcum = clamp(colorAcum, vec4(0.0,0.0,0.0,0.0), vec4(1.0,1.0,1.0,1.0));\n";
  shader += " outColor = colorAcum;\n";

  shader += "}\n";

  return shader;
}

QString
RcShaderFactory::genEdgeEnhanceShader()
{
  QString shader;

  shader += "#version 450 core\n";
  
  shader += "uniform sampler2DRect normalTex;\n";
  shader += "uniform sampler2DRect pvtTex;\n";
  shader += "uniform float sceneRadius;\n";
  //shader += "uniform float minZ;\n";
  //shader += "uniform float maxZ;\n";
  shader += "uniform float dzScale;\n";
  shader += "uniform float isoshadow;\n";
  shader += "uniform float gamma;\n";
  shader += "uniform float roughness;\n";
  shader += "uniform float specular;\n";

  shader += "out vec4 glFragColor;\n";

  shader += ShaderFactory::ggxShader();

  shader += "void main(void)\n";
  shader += "{\n";
  //shader += "  glFragColor = vec4(0.0);\n";
  
  shader += "  vec2 spos0 = gl_FragCoord.xy;\n";

  shader += "  vec4 color = texture(pvtTex, spos0);\n";
  shader += "  vec3 dvt = texture(normalTex, spos0).xyz;\n";

  shader += "  gl_FragDepth = dvt.x;\n";
  shader += "  glFragColor = color;\n";
  
  //---------------------
  shader += "  if (color.a < 0.001)\n";
  shader += "    discard;\n";
  //shader += "  color /= color.a;\n";
  //---------------------

//  shader += "  if (all(lessThan(vec2(dzScale,isoshadow), vec2(0.001))))\n"; // no post processing
//  shader += "    return;\n";

  //------------------------------------
  // apply gamma correction
  shader += "      color.rgb = pow(color.rgb, vec3(gamma*gamma));\n";
  shader += "      color = clamp(color, vec4(0.0,0.0,0.0,0.0), vec4(1.0,1.0,1.0,1.0));\n";
  //------------------------------------

  
  shader+= " if (isoshadow > 0.0)\n"; // soft shadows
  shader+= " {\n";
  shader += "  float depth = dvt.z;\n";
  shader+= "   float sum = 0.0;\n";
  shader+= "   float delta = max(0.1, isoshadow*0.75);\n";
  shader+= "   int nsteps = int(30.0*isoshadow);\n";
  shader+= "   for(int i=0; i<nsteps; i++)\n";
  shader+= "    {\n";
  shader += "     float r = 1.0+i*delta;\n";
  shader += "     float x = r*sin(radians(i*41));\n";
  shader += "     float y = r*cos(radians(i*41));\n";
  shader += "     vec2 pos = spos0 + vec2(x,y);\n";
  shader+= "	  float od = (depth - texture2DRect(normalTex, pos).z)/sceneRadius;\n";
  shader += "     sum += step(0.01, od);\n";
  shader+= "    } \n";
  shader += "    sum /= float(nsteps);\n";
  shader += "    float shadow = clamp(sum,0.0,1.0);\n";
  shader += "    float shadows = max(0.1, exp(-shadow));\n";
  shader += "    color.rgb *= pow(shadows, gamma);\n";
  shader+= " }\n";

  

  shader += "  if (dzScale > 0.0)\n"; // edges
  shader += "  {\n";
  shader += "    float response = 0.0;\n";
  shader += "    for(int i=0; i<8; i++)\n";
  shader += "      {\n";
  shader += "        float r = 1.0;\n";
  shader += "        float x = r*sin(radians(i*45));\n";
  shader += "        float y = r*cos(radians(i*45));\n";
  shader += "        vec2 pos = spos0 + vec2(x,y);\n";
  shader += "        float adepth = texture2DRect(normalTex, pos).z;\n";
  shader += "        float od = pow((dvt.z - adepth)/sceneRadius, 1.2);\n";
  shader += "        response += max(0.0, abs(od));\n";
  shader += "      } \n";
  shader += "    color.rgb *= pow(exp(-20*response/gamma), dzScale);\n";
  shader += "  }\n";

  
  shader += "  if (roughness > 0.0)\n";
  shader += "  {\n";
  shader += "    float dx = 0.0;\n";
  shader += "    float dy = 0.0;\n";
  shader += "    for(int i=0; i<10*gamma; i++)\n";
  shader += "      {\n";
  shader += "        float offset = 1-(1+i)%3;\n";
  shader += "        dx += texture2DRect(normalTex, spos0.xy+vec2(1+i,offset)).z -\n";
  shader += "              texture2DRect(normalTex, spos0.xy-vec2(1+i,offset)).z;\n";
  shader += "        dy += texture2DRect(normalTex, spos0.xy+vec2(offset,1+i)).z -\n";
  shader += "              texture2DRect(normalTex, spos0.xy-vec2(offset,1+i)).z;\n";
  shader += "      }\n";
  shader += "    vec3 N = normalize(vec3(dx/sceneRadius, dy/sceneRadius, roughness));\n";
  //shader += "    vec3 spec = shadingSpecularGGX(N, vec3(0,0,1),  vec3(0,0,1), roughness*0.1, color.rgb);\n";
  //shader += "    color.rgb += 0.5*specular*spec;\n";
  shader += "    float specCoeff = 512.0/mix(0.1, 64.0, roughness*roughness);\n";
  shader += "    float shine = specular * pow(N.z, specCoeff);\n";
  shader += "    color.rgb = mix(color.rgb, pow(color.rgb, vec3(1.0-shine)), vec3(shine));\n";
  shader += "  }\n";
  


  //------------------------------------
  // apply gamma correction
  shader += "      color.rgb = pow(color.rgb, vec3(1.0/gamma));\n";
  shader += "      color = clamp(color, vec4(0.0,0.0,0.0,0.0), vec4(1.0,1.0,1.0,1.0));\n";
  //------------------------------------

  
    
  shader += " if (any(greaterThan(color.rgb,vec3(1.0)))) \n";
  shader += "   color.rgb = vec3(1.0);\n";
  shader += "  glFragColor = color;\n";

  shader += "}\n";

  return shader;
}
//----------------------------
//----------------------------

GLint RcShaderFactory::m_boxShaderParm[20];
GLint* RcShaderFactory::boxShaderParm() { return &m_boxShaderParm[0]; }

GLuint RcShaderFactory::m_boxShader = 0;
GLuint RcShaderFactory::boxShader()
{
  if (!m_boxShader)
    {
      m_boxShader = glCreateProgram();
      QString vertShaderString = boxShaderV();
      QString fragShaderString = boxShaderF();
  
      bool ok = loadShader(m_boxShader,
			   vertShaderString,
			   fragShaderString);  

      if (!ok)
	{
	  QMessageBox::information(0, "", "Cannot load box shaders");
	  return 0;
	}
	
	m_boxShaderParm[0] = glGetUniformLocation(m_boxShader, "MVP");

    }

  return m_boxShader;
}


QString
RcShaderFactory::boxShaderV()
{
  QString shader;

  shader += "#version 410\n";
  shader += "uniform mat4 MVP;\n";
  shader += "layout(location = 0) in vec3 position;\n";
  shader += "out vec3 v3Color;\n";
  shader += "void main()\n";
  shader += "{\n";
  shader += "   v3Color = position;\n";
  shader += "   gl_Position = MVP * vec4(position, 1);\n";
  shader += "}\n";

  return shader;
}

QString
RcShaderFactory::boxShaderF()
{
  QString shader;

  shader += "#version 410 core\n";
  shader += "in vec3 v3Color;\n";
  shader += "out vec4 outputColor;\n";
  shader += "void main()\n";
  shader += "{\n";
  shader += "  outputColor = vec4(v3Color,1);\n";
  shader += "}\n";

  return shader;
}

bool
RcShaderFactory::loadShader(GLhandleARB &progObj,
			    QString vertShaderString,
			    QString fragShaderString)
{
  GLhandleARB fragObj = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);  
  glAttachObjectARB(progObj, fragObj);

  GLhandleARB vertObj = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);  
  glAttachObjectARB(progObj, vertObj);

  {  // vertObj   
    int len = vertShaderString.length();
    char *tbuffer = new char[len+1];
    sprintf(tbuffer, vertShaderString.toLatin1().data());
    const char *sstr = tbuffer;
    glShaderSourceARB(vertObj, 1, &sstr, NULL);
    delete [] tbuffer;

    GLint compiled = -1;
    glCompileShaderARB(vertObj);
    glGetObjectParameterivARB(vertObj,
			      GL_OBJECT_COMPILE_STATUS_ARB,
			      &compiled);
    if (!compiled)
      {
	GLcharARB str[1000];
	GLsizei len;
	glGetInfoLogARB(vertObj,
			(GLsizei) 1000,
			&len,
			str);
	
	QString estr;
	QStringList slist = vertShaderString.split("\n");
	for(int i=0; i<slist.count(); i++)
	  estr += QString("%1 : %2\n").arg(i+1).arg(slist[i]);
	
	QTextEdit *tedit = new QTextEdit();
	tedit->insertPlainText("-------------Error----------------\n\n");
	tedit->insertPlainText(str);
	tedit->insertPlainText("\n-----------Shader---------------\n\n");
	tedit->insertPlainText(estr);
	
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(tedit);
	
	QDialog *showError = new QDialog();
	showError->setWindowTitle("Error in Vertex Shader");
	showError->setSizeGripEnabled(true);
	showError->setModal(true);
	showError->setLayout(layout);
	showError->exec();
	return false;
    }
  }
    
    
  { // fragObj
    int len = fragShaderString.length();
    char *tbuffer = new char[len+1];
    sprintf(tbuffer, fragShaderString.toLatin1().data());
    const char *sstr = tbuffer;
    glShaderSourceARB(fragObj, 1, &sstr, NULL);
    delete [] tbuffer;
  
    GLint compiled = -1;
    glCompileShaderARB(fragObj);
    glGetObjectParameterivARB(fragObj,
			      GL_OBJECT_COMPILE_STATUS_ARB,
			      &compiled);
	
    if (!compiled)
      {
	GLcharARB str[1000];
	GLsizei len;
	glGetInfoLogARB(fragObj,
			(GLsizei) 1000,
			&len,
			str);
	
	//-----------------------------------
	// display error
	
	//qApp->beep();
	
	QString estr;
	QStringList slist = fragShaderString.split("\n");
	for(int i=0; i<slist.count(); i++)
	  estr += QString("%1 : %2\n").arg(i+1).arg(slist[i]);
	
	QTextEdit *tedit = new QTextEdit();
	tedit->insertPlainText("-------------Error----------------\n\n");
	tedit->insertPlainText(str);
	tedit->insertPlainText("\n-----------Shader---------------\n\n");
	tedit->insertPlainText(estr);
	
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(tedit);
	
	QDialog *showError = new QDialog();
	showError->setWindowTitle("Error in Fragment Shader");
	showError->setSizeGripEnabled(true);
	showError->setModal(true);
	showError->setLayout(layout);
	showError->exec();
	//-----------------------------------
	
	return false;
      }
  }

  
  //----------- link program shader ----------------------
  GLint linked = -1;
  glLinkProgramARB(progObj);
  glGetObjectParameterivARB(progObj, GL_OBJECT_LINK_STATUS_ARB, &linked);
  if (!linked)
    {
      GLcharARB str[1000];
      GLsizei len;
      QMessageBox::information(0,
			       "ProgObj",
			       "error linking texProgObj");
      glGetInfoLogARB(progObj,
		      (GLsizei) 1000,
		      &len,
		      str);
      QMessageBox::information(0,
			       "Error",
			       QString("%1\n%2").arg(len).arg(str));
      return false;
    }

  glDeleteObjectARB(fragObj);
  glDeleteObjectARB(vertObj);

  return true;
}
//---------------
