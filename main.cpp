#include <ysshellext.h>
#include <stdio.h>
#include <time.h>
#include <vector>

#include <ysclass.h>
#include <ysgl.h>
#include <ysglcpp.h>
#include <ysglslcpp.h>

#include <fslazywindow.h>

#include "vertex_buffer_object.h"
#include "lattice.h"
#include "hashtable.h"

#include "Proxy.h"
#include "VsaQueue.h"
#include "Pqueue.h"

class FsLazyWindowApplication : public FsLazyWindowApplicationBase
{
protected:
	enum STATE
	{
		STATE_MAKEINITIAL,
		STATE_ASSIGNPOLY,
		STATE_LASER
	};

	STATE state;
	bool needRedraw;

	Ys3DDrawingEnvironment drawEnv;


	int prevMx,prevMy;

	// FsLazyWindow
	YsShellExt shl;
	std::vector <float> vtx,nom;
	std::vector <float> col;
	YsVec3 min,max;



	// VSA
	std::vector<Proxy> proxy;
	std::vector<YsShell::PolygonHandle>temppoly;
	int numberOfProxies;
	HashTable <Proxy,std::vector <YsShell::PolygonHandle> > proxyToTriangles;
	HashTable <YSHASHKEY,int> polygonToLabel;
	


	YsVec3 lastClick[2];
	// Need to go to each polygon and check the neighbours 
	// Out of 3 neighbours if any neighbour has a proxy number that is different other than th current polygon that consider that edge.
	// If any of that edge has a common vertex then it is bound to be a anchor vertex


public:
	// Display Functions
	void CacheBoundingBox(void);
	static void GetBoundingBox(YsVec3 &min,YsVec3 &max,const std::vector <float> &vtx);
	static void PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx);
	static void PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx,int y);
	static void PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx,int x,int y);
	static void VtxNomToYsShell(YsShellExt &shl,const std::vector <float> &vtx,const std::vector <float> &nom);
	static void YsShellToVtxNom(std::vector <float> &vtx,std::vector <float> &nom,std::vector <float> &col,const YsShellExt &shl);

	// Llyod Clustering
	void makeInitialProxies();
	double calculateError(YsShell::PolygonHandle plHd,Proxy pxy);
	double calculateAngle(YsShell::PolygonHandle plHd,YsVec3 normal);
	void setupPolygonToLabelHashTable();
	void generateProxyAssociatedTriangle();
	std::vector<Proxy> getNewProxies();
	bool checkConvergence(std::vector<Proxy> newProxies);
	void setupProxyToPolygonHashTable();
	void makeCluster();
	void updateColour();
	bool checkIfHashTableFilled();


	// Inclas function
	FsLazyWindowApplication();
	virtual void BeforeEverything(int argc,char *argv[]);
	virtual void GetOpenWindowOption(FsOpenWindowOption &OPT) const;
	virtual void Initialize(int argc,char *argv[]);
	virtual void Interval(void);
	virtual void BeforeTerminate(void);
	virtual void Draw(void);
	virtual bool UserWantToCloseProgram(void);
	virtual bool MustTerminate(void) const;
	virtual long long int GetMinimumSleepPerInterval(void) const;
	virtual bool NeedRedraw(void) const;
};
// Hashcode for the Hastables
template <>
unsigned long long int HashTable<Proxy,std::vector<YsShell::PolygonHandle>>::HashCode(const Proxy &key) const
{		
	return key.getNumber();
}

template <>
unsigned long long int HashTable<YSHASHKEY,int>::HashCode(const YSHASHKEY &key) const
{		
	return key;
}
// VSA Functions
void FsLazyWindowApplication::makeInitialProxies(void)
{
	// Intialize the time for the algorithm
	srand(time(NULL));
	// Create the hashtable to check the visited polygons
	YsShellPolygonStore visited(shl.Conv());
	std::vector<YsShell::PolygonHandle> polyhd;
	int n = shl.GetNumPolygon();
	// Create an array that contains all the polygons
	for(auto plHd:shl.AllPolygon())
	{
		polyhd.push_back(plHd);
	}
	// Clear the visited Hashtable
	visited.CleanUp();
	// For each Proxy
	for (int i = 0; i < numberOfProxies;)
	{
		// Get the random number
		int l = rand()%n;
		// Get the random Polygon
		auto plHd = polyhd[l];
		// IF it is included or is a nullptr, then find another polygon
		if (YSTRUE!=visited.IsIncluded(plHd) && plHd!=nullptr)
		{
			// If it is not included and is not  a nullptr
			// Add it to the visited list
			visited.Add(plHd);
			// Update the porxy for this polygon
			polygonToLabel.Update(shl.GetSearchKey(plHd),i);
			//printf("Lable %d has been set for the random number %d\n",i,l);
			// Create a local Temporary proxy
			Proxy tempProxy;
			tempProxy.setNumber(i);
			tempProxy.setPolygon(plHd);
			temppoly.push_back(plHd);
			// Push it to the gobal proxy
			proxy.push_back(tempProxy);
			// Go to the next proxy
			i++;			
		}
	}
	printf("Initial Proxies have been set\n");
	// Update the colour for this process
	updateColour();
}
void FsLazyWindowApplication::setupPolygonToLabelHashTable(void)
{
	// For each polygon make the Proxy to be -1
	for(auto plHd:shl.AllPolygon())
	{
		polygonToLabel.Update(shl.GetSearchKey(plHd),-1);
	}
	printf("PolygonToLabelHashTable has been setup\n");
}
double FsLazyWindowApplication::calculateError(YsShell::PolygonHandle plHd,Proxy pxy)
{
	// L2,1 norm
	YsVec3 n = shl.GetNormal(plHd) - shl.GetNormal(pxy.getPolygon());
	return n.GetSquareLength()*shl.GetPolygonArea(plHd);
}
double FsLazyWindowApplication::calculateAngle(YsShell::PolygonHandle plHd,YsVec3 normal)
{
	// Get Angle between the normal and the Triangle
	return acos(shl.GetNormal(plHd)*normal);
}
bool FsLazyWindowApplication::checkIfHashTableFilled()
{
	for(auto plHd : shl.AllPolygon())
	{
		if (*polygonToLabel[shl.GetSearchKey(plHd)]==-1)
		{
			printf("There exists a polygon that has label not yet assigned\n");
			return false;
		}
	}
	return true;
}
void FsLazyWindowApplication::generateProxyAssociatedTriangle()
{
	// Create the Queue
	PriorityQueue <VsaQueue> queue;
	// For ecah proxy
	for(int i=0;i<numberOfProxies;i++)
	{
		// Get the representation polygon
		auto plHd = proxy[i].getPolygon();
		// Get the number of neighbours
		auto numNeighbours =  shl.GetPolygonNumVertex(proxy[i].getPolygon());
		// For each neighbour
		for(int j=0;j<numNeighbours;j++)
		{
			// Get the Neighbouring Polygon
			auto neiplHd = shl.GetNeighborPolygon(plHd,j);
			// If the neighbour does exist
			if (neiplHd!=nullptr)
			{
				// If there is not label for the polygon then push it in the PQ
				if(*polygonToLabel[shl.GetSearchKey(neiplHd)]==-1)
				{
					VsaQueue vsa;
					auto error = calculateError(neiplHd,proxy[i]);
					vsa.setError(error);
					vsa.setHandle(neiplHd);
					vsa.setProxyTestLabel(i);
					queue.push_back(vsa);
				}
			}
		}
	}
	// Till all the polygons have not been given the proxy
	while(!queue.isEmpty())
	{
		// Get the polygon with the least error
		VsaQueue vsa = queue.top();
		queue.pop_back();
		//IF no label has been assgined yet
		if(*polygonToLabel[shl.GetSearchKey(vsa.getHandle())]==-1)
		{
			// Assign the label that it is being tested against
			polygonToLabel.Update(shl.GetSearchKey(vsa.getHandle()),vsa.getProxyTestLabel());
			// Get the Polygon
			auto plHd = vsa.getHandle();
			// Get the number of neighbours
			auto numNeighbours =  shl.GetPolygonNumVertex(plHd);
			// For each neighbours
			for(int j=0;j<numNeighbours;j++)
			{
				// Get the neighbours
				auto neiplHd = shl.GetNeighborPolygon(plHd,j);
				if (neiplHd!=nullptr)
				{
					if(*polygonToLabel[shl.GetSearchKey(neiplHd)]==-1)
					{
						VsaQueue tempVsa;
						auto error = calculateError(neiplHd,proxy[vsa.getProxyTestLabel()]);
						tempVsa.setError(error);
						tempVsa.setHandle(neiplHd);
						tempVsa.setProxyTestLabel(vsa.getProxyTestLabel());
						queue.push_back(tempVsa);
					}
					else
					{
						printf("There does not exist a neighhbour for this polygon\n");
					}
				}
			}
		}
	}
	// Update the colour based on the new association
	updateColour();
	printf("generated associated proxy triangles\n");
	// Check if all the polygons have been given  a colour 
	// 10/16/2016  When Cesna file was used since there are independent bodies associated with it it was difficult to get the algorithm to passs
	// If A Proxy has a normal in close inclination with a body from another shell which would always be -1 since it would never get added  to Queue being an independent body
	if(!checkIfHashTableFilled())
	{
		printf("There was a polygon that did not have a label assigned , the algotihm might fail it you proceed");
	}
}
std::vector<Proxy> FsLazyWindowApplication::getNewProxies()
{
	// Data Structure for NewProxies
	std::vector<Proxy> newProxies;
	newProxies.resize(numberOfProxies);
	// Data Structure to store the sum of normals of all the polygons in each proxy
	std::vector<YsVec3> normalSum;
	normalSum.resize(numberOfProxies);
	// Data Structure to store the sum of Areas of all the polygons in each proxy
	std::vector<double> areaSum;
	areaSum.resize(numberOfProxies);
	// Data Structure to sotre the sum of centre of all the polygons in each proxy
	std::vector<YsVec3> centreSum;
	centreSum.resize(numberOfProxies);
	// Data Structure for storing the number of polygons in each proxy
	std::vector<int> countSum;
	countSum.resize(numberOfProxies);
	// Data Structure for NewProxies
	std::vector<double> minAngle;
	minAngle.resize(numberOfProxies,2*YsPi);
	// Loop through each polygon and add its normal, area, centre and count to the Proxy that it is associated with
	for (auto plHd : shl.AllPolygon())
	{
		if(polygonToLabel[shl.GetSearchKey(plHd)]!=nullptr)
		{
			int loc = *polygonToLabel[shl.GetSearchKey(plHd)];
			normalSum[loc]+=shl.GetNormal(plHd);
			areaSum[loc]+=shl.GetPolygonArea(plHd);
			centreSum[loc]+=shl.GetCenter(plHd);
			countSum[loc]++;
		}
		else
		{
			printf("You should not be seing this message\n");
		}
	}
	// Get the representation of new Proxies
	for (int i = 0; i < numberOfProxies; i++)
	{
		normalSum[i]/=areaSum[i];
		centreSum[i]/=countSum[i];
	}
	for (int i = 0; i < normalSum.size(); i++)
	{
		printf("Normal values are [%0.2lf][%0.2lf][%0.2lf] and the centre value is [%0.2lf][%0.2lf][%0.2lf]\n",normalSum[i].x(),normalSum[i].y(),normalSum[i].z(),centreSum[i].x(),centreSum[i].y(),centreSum[i].z());
	}
	// Find the best polygon for each new proxy
	for (auto plHd : shl.AllPolygon())
	{
		if(polygonToLabel[shl.GetSearchKey(plHd)]!=nullptr)
		{
			int localPxy =*polygonToLabel[shl.GetSearchKey(plHd)];
			auto angle = calculateAngle(plHd,normalSum[localPxy]);
			if (angle<minAngle[localPxy])
			{
				minAngle[localPxy]= angle;
				newProxies[localPxy].setPolygon(plHd);
				newProxies[localPxy].setNumber(localPxy);
			}
		}
		else
		{
			printf("You should not be seing this message\n");
		}
	}
	return newProxies;
}

bool FsLazyWindowApplication::checkConvergence(std::vector<Proxy> newProxies)
{
	// If for any of the proxies the polygon that represents it has changed then we have a problem
	for (int i = 0; i < numberOfProxies; i++)
	{
		if(proxy[i].getPolygon()!=newProxies[i].getPolygon())
			return false;

	}
	return true;
}

void FsLazyWindowApplication::setupProxyToPolygonHashTable(void)
{
	// After all the iterations have been done we need to make a hashtable of Proxy to Polygon
	for(auto plHd:shl.AllPolygon())
	{
		if(polygonToLabel[shl.GetSearchKey(plHd)]!=nullptr)
		{
			int localPxy =*polygonToLabel[shl.GetSearchKey(plHd)];
			if (proxyToTriangles[proxy[localPxy]]==nullptr)
			{
				printf("%d th proxy was empty assigning a std::vector now");
				std::vector<YsShell::PolygonHandle> triangles;
				triangles.push_back(plHd);
				proxyToTriangles.Update(proxy[localPxy],triangles);
			}
			else
			{
				proxyToTriangles[proxy[localPxy]]->push_back(plHd);
			}
		}
		else
		{
			printf("You should not be seing this message\n");
		}
	}
}
void FsLazyWindowApplication::makeCluster(void)
{
	// Initalize the Hashtable
	setupPolygonToLabelHashTable();
	// Make the initial Proxies
	makeInitialProxies();
	bool terminate = true;
	do
	{
		// get the association of Triangles with the Proxies
		generateProxyAssociatedTriangle();
		// get the new proxy
		std::vector<Proxy> newPrxy = getNewProxies();
		// Check if there is a difference between the two iterations
		if (!checkConvergence(newPrxy))
		{
			setupPolygonToLabelHashTable();
			for (int i = 0; i < proxy.size(); i++)
			{
				proxy[i].setPolygon(newPrxy[i].getPolygon());
				polygonToLabel.Update(shl.GetSearchKey(proxy[i].getPolygon()),i);
			}
		}
		else
		{
			// There is no difference , it should terminate now
			terminate = false;
		}
	} while (terminate);
	// Need to make sure that after the termination of the Checker there is not Polygon tha has not yet been assigned anything 
	// This can be really detrimental for the further section of the algorithm
	if (!checkIfHashTableFilled())
	{
		printf("There is something seriously wrong with the Algorithm and We need to fix it right now !!!!!!!!!!!\n");
	}
	std::vector<std::vector<YsShell::PolygonHandle>> proxyToPoly;
	proxyToPoly.resize(numberOfProxies);
	// Create the Table of Proxies to Polgons
	for(auto plHd : shl.AllPolygon())
	{
		int loc  = *polygonToLabel[shl.GetSearchKey(plHd)];
		proxyToPoly[loc].push_back(plHd);
	}
	// Use this table to populate the Hashtable
	for (int i = 0; i < numberOfProxies; i++)
	{
		proxyToTriangles.Update(proxy[i],proxyToPoly[i]);
	}
}
// Need to be changed since it does not have all the colours or the 4 colour mapping

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// NEED TO IMPLEMENT 4 COLOUR MAPPING
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void FsLazyWindowApplication::updateColour(void)
{
	for(auto plHd :shl.AllPolygon())
	{
		switch (*polygonToLabel[shl.GetSearchKey(plHd)]%5)
		{
		case -1:
			shl.SetPolygonColor(plHd,YsWhite());
			break;
		case 0:
			shl.SetPolygonColor(plHd,YsRed());
			break;
		case 1:
			shl.SetPolygonColor(plHd,YsBlue());
			break;
		case 2:
			shl.SetPolygonColor(plHd,YsYellow());
			break;
		case 3:
			shl.SetPolygonColor(plHd,YsBlack());
			break;
		case 4:
			shl.SetPolygonColor(plHd,YsGreen());
			break;
		case 5:
			shl.SetPolygonColor(plHd,YsCyan());
			break;
		default:
			break;
		}		
	}
	YsShellToVtxNom(vtx,nom,col,shl);
}



// FsLazyWindow functions
void FsLazyWindowApplication::CacheBoundingBox(void)
{
	shl.GetBoundingBox(min,max);
}

/* static */ void FsLazyWindowApplication::GetBoundingBox(YsVec3 &min,YsVec3 &max,const std::vector <float> &vtx)
{
	auto nVtx=vtx.size()/3;
	if(0==nVtx)
	{
		min=YsVec3::Origin();
		max=YsVec3::Origin();
	}
	else
	{
		YsBoundingBoxMaker3 mkBbx;
		for(decltype(nVtx) i=0; i<nVtx; ++i)
		{
			YsVec3 pos(vtx[i*3],vtx[i*3+1],vtx[i*3+2]);
			mkBbx.Add(pos);
		}
		mkBbx.Get(min,max);
	}
}

/* static */ void FsLazyWindowApplication::PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx)
{
	YsVec3 min,max;
	GetBoundingBox(min,max,vtx);
	double d=(max-min).GetLength()/100.0;
	min-=YsXYZ()*d; // Make absolutely sure that all vertices are inside.
	max+=YsXYZ()*d;

	auto nVtx=vtx.size()/3;
	ltc.Create(100,100,100);
	ltc.SetDimension(min,max);
}
/* static */ void FsLazyWindowApplication::PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx,int y)
{
	YsVec3 min,max;
	GetBoundingBox(min,max,vtx);
	double d=(max-min).GetLength()/100.0;
	//min-=YsXYZ()*d; // Make absolutely sure that all vertices are inside.
	//max+=YsXYZ()*d;

	auto nVtx=vtx.size()/3;
	ltc.Create(1,y,1);
	ltc.SetDimension(min,max);
}
/* static */ void FsLazyWindowApplication::PrepareLatticeForConnection(Lattice3d <std::vector <YsShell::VertexHandle> > &ltc,const std::vector <float> &vtx,int x,int y)
{
	YsVec3 min,max;
	GetBoundingBox(min,max,vtx);
	double d=(max-min).GetLength()/100.0;
	//min-=YsXYZ()*d; // Make absolutely sure that all vertices are inside.
	//max+=YsXYZ()*d;

	auto nVtx=vtx.size()/3;
	ltc.Create(x,y,1);
	ltc.SetDimension(min,max);
}

/* static */ void FsLazyWindowApplication::VtxNomToYsShell(YsShellExt &shl,const std::vector <float> &vtx,const std::vector <float> &nom)
{
	Lattice3d <std::vector <YsShell::VertexHandle> > ltc;
	PrepareLatticeForConnection(ltc,vtx);

	shl.CleanUp();
	for(int i=0; i<vtx.size()/9; ++i)
	{
		const YsVec3 vtNom(nom[i*9],nom[i*9+1],nom[i*9+2]);
		const YsVec3 vtPos[3]=
		{
			YsVec3(vtx[i*9  ],vtx[i*9+1],vtx[i*9+2]),
			YsVec3(vtx[i*9+3],vtx[i*9+4],vtx[i*9+5]),
			YsVec3(vtx[i*9+6],vtx[i*9+7],vtx[i*9+8]),
		};
		YsShell::VertexHandle vtHd[3];
		for(int i=0; i<3; ++i)
		{
			vtHd[i]=nullptr;
			auto idx=ltc.GetBlockIndex(vtPos[i]);
			if(true==ltc.IsInRange(idx))
			{
				for(auto tstVtHd : ltc.Elem(idx.x(),idx.y(),idx.z()))
				{
					if(shl.GetVertexPosition(tstVtHd)==vtPos[i])
					{
						vtHd[i]=tstVtHd;
						break;
					}
				}
			}
			if(nullptr==vtHd[i])
			{
				vtHd[i]=shl.AddVertex(vtPos[i]);
				if(true==ltc.IsInRange(idx))
				{
					ltc.Elem(idx.x(),idx.y(),idx.z()).push_back(vtHd[i]);
				}
			}
		}
		YsShell::PolygonHandle plHd;
		plHd=shl.AddPolygon(3,vtHd);
		shl.SetPolygonNormal(plHd,vtNom);
		shl.SetPolygonColor(plHd,YsBlue());
	}
}
/* static */ void FsLazyWindowApplication::YsShellToVtxNom(std::vector <float> &vtx,std::vector <float> &nom,std::vector <float> &col,const YsShellExt &shl)
{
	vtx.clear();
	nom.clear();
	col.clear();
	for(auto plHd : shl.AllPolygon())
	{
		auto plVtHd=shl.GetPolygonVertex(plHd);
		if(3<=plVtHd.GetN())
		{
			auto plNom=shl.GetNormal(plHd);
			auto plCol=shl.GetColor(plHd);
			for(auto vtHd : plVtHd)
			{
				auto vtPos=shl.GetVertexPosition(vtHd);
				vtx.push_back(vtPos.xf());
				vtx.push_back(vtPos.yf());
				vtx.push_back(vtPos.zf());
				nom.push_back(plNom.xf());
				nom.push_back(plNom.yf());
				nom.push_back(plNom.zf());
				col.push_back(plCol.Rf());
				col.push_back(plCol.Gf());
				col.push_back(plCol.Bf());
				col.push_back(1);
			}
		}
	}
}


FsLazyWindowApplication::FsLazyWindowApplication()
{
	drawEnv.SetProjectionMode(YsProjectionTransformation::PERSPECTIVE);
	drawEnv.SetViewTarget(YsVec3::Origin());
	drawEnv.SetViewDistance(20.0);
	drawEnv.SetViewAttitude(YsAtt3(YsPi/4.0,YsPi/5.0,0.0));

	prevMx=0;
	prevMy=0;

	needRedraw=false;

	lastClick[0]=YsVec3::Origin();
	lastClick[1]=YsVec3::Origin();

}

/* virtual */ void FsLazyWindowApplication::BeforeEverything(int argc,char *argv[])
{
	if(2<=argc)
	{
		shl.LoadStl(argv[1]);
		if (argc==2)
		{
			numberOfProxies = 6;
		}
		else
		{
			numberOfProxies = atoi(argv[2]);
		}
	}
	shl.EnableSearch();
	makeCluster();
	CacheBoundingBox();
}
/* virtual */ void FsLazyWindowApplication::GetOpenWindowOption(FsOpenWindowOption &opt) const
{
	opt.x0=0;
	opt.y0=0;
	opt.wid=1200;
	opt.hei=800;
}
/* virtual */ void FsLazyWindowApplication::Initialize(int argc,char *argv[])
{
	YsGLSLRenderer::CreateSharedRenderer();
}
/* virtual */ void FsLazyWindowApplication::Interval(void)
{
	int wid,hei;
	FsGetWindowSize(wid,hei);

	int lb,mb,rb,mx,my;
	auto evt=FsGetMouseEvent(lb,mb,rb,mx,my);
	if(0!=lb && (mx!=prevMx || my!=prevMy))
	{
		double denom=(double)YsGreater(wid,hei);
		double dx=2.0*(double)(prevMx-mx)/denom;
		double dy=2.0*(double)(prevMy-my)/denom;
		drawEnv.RotateView(dx,dy);
	}

	

	prevMx=mx;
	prevMy=my;

	auto key=FsInkey();
	if(FSKEY_ESC==key)
	{
		SetMustTerminate(true);
	}
	if(FSKEY_P==key)
	{
		setupPolygonToLabelHashTable();
		makeInitialProxies();
	}
	if(FSKEY_Q==key)
	{
		generateProxyAssociatedTriangle();
	}
	needRedraw=true;
}
/* virtual */ void FsLazyWindowApplication::BeforeTerminate(void)
{
	YsGLSLRenderer::DeleteSharedRenderer();
}
/* virtual */ void FsLazyWindowApplication::Draw(void)
{
	int wid,hei;
	FsGetWindowSize(wid,hei);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	drawEnv.SetProjectionMode(YsProjectionTransformation::PERSPECTIVE);
	drawEnv.SetAspectRatio((double)wid/(double)hei);
	drawEnv.SetFOVY(YsPi/4.0);
	drawEnv.SetNearFar(0.1,100.0);
	drawEnv.SetViewTarget((min+max)/2.0);
	drawEnv.SetViewDistance((max-min).GetLength()/1.0);

	GLfloat projMat[16];
	drawEnv.GetProjectionMatrix().GetOpenGlCompatibleMatrix(projMat);


	auto &view=drawEnv.GetViewMatrix();

	YsMatrix4x4 modeling;

	YsMatrix4x4 fullMatrix=view*modeling;

	GLfloat viewMat[16];
	fullMatrix.GetOpenGlCompatibleMatrix(viewMat);
	
	if(true)
	{
		GLfloat lightDir[]={0,0,1};
		YsGLSLShaded3DRenderer renderer;  // Again, do not nest the renderer!
		renderer.SetProjection(projMat);
		renderer.SetModelView(viewMat);
		renderer.SetLightDirectionInCameraCoordinate(0,lightDir);

		renderer.DrawVtxNomCol(GL_TRIANGLES,vtx.size()/3,vtx.data(),nom.data(),col.data());
	}	
	YsMatrix4x4 shadowMat;
	shadowMat.Translate(0.0,-12.0,0.0);
	shadowMat.Scale(1.0,0.0,1.0);

	fullMatrix=view*shadowMat*modeling;
	fullMatrix.GetOpenGlCompatibleMatrix(viewMat);
	{
		GLfloat color[]={0,0,0,1};

		YsGLSLPlain3DRenderer renderer;  // Again, do not nest the renderer!
		renderer.SetProjection(projMat);
		renderer.SetModelView(viewMat);
		renderer.SetUniformColor(color);
		renderer.DrawVtx(GL_TRIANGLES,vtx.size()/3,vtx.data());
	}


	FsSwapBuffers();

	needRedraw=false;
}
/* virtual */ bool FsLazyWindowApplication::UserWantToCloseProgram(void)
{
	return true; // Returning true will just close the program.
}
/* virtual */ bool FsLazyWindowApplication::MustTerminate(void) const
{
	return FsLazyWindowApplicationBase::MustTerminate();
}
/* virtual */ long long int FsLazyWindowApplication::GetMinimumSleepPerInterval(void) const
{
	return 10;
}
/* virtual */ bool FsLazyWindowApplication::NeedRedraw(void) const
{
	return needRedraw;
}


static FsLazyWindowApplication *appPtr=nullptr;

/* static */ FsLazyWindowApplicationBase *FsLazyWindowApplicationBase::GetApplication(void)
{
	if(nullptr==appPtr)
	{
		appPtr=new FsLazyWindowApplication;
	}
	return appPtr;
}
