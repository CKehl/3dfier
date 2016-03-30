/*
 Copyright (c) 2015 Hugo Ledoux
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

#include "Map3d.h"
#include "io.h"


Map3d::Map3d() {
  OGRRegisterAll();
  _building_include_floor = false;
  _building_heightref_roof = "median";
  _building_heightref_floor = "percentile-05";
  _building_triangulate = true;
  _terrain_simplification = 0;
  _forest_simplification = 0;
  _radius_vertex_elevation = 1.0;
  _threshold_jump_edges = 0.5;
  _bbox.min_corner().set<0>(1e8);
  _bbox.min_corner().set<1>(1e8);
  _bbox.max_corner().set<0>(-1e8);
  _bbox.max_corner().set<1>(-1e8);
}


Map3d::~Map3d() {
  // TODO : destructor Map3d
  _lsFeatures.clear();
}

void Map3d::set_building_heightref_roof(std::string h) {
  _building_heightref_roof = h;  
}

void Map3d::set_building_heightref_floor(std::string h) {
  _building_heightref_floor = h;  
}


void Map3d::set_radius_vertex_elevation(float radius) {
  _radius_vertex_elevation = radius;  
}

void Map3d::set_threshold_jump_edges(float threshold) {
  _threshold_jump_edges = threshold;  
}

void Map3d::set_building_include_floor(bool include) {
  _building_include_floor = include;
}

void Map3d::set_building_triangulate(bool triangulate) {
  _building_triangulate = triangulate;
}

void Map3d::set_terrain_simplification(int simplification) {
  _terrain_simplification = simplification;
}

void Map3d::set_forest_simplification(int simplification) {
  _forest_simplification = simplification;
}

void Map3d::set_water_heightref(std::string h) {
  _water_heightref = h;  
}

void Map3d::set_road_heightref(std::string h) {
  _road_heightref = h;  
}

std::string Map3d::get_citygml() {
  std::stringstream ss;
  ss << get_xml_header();
  ss << get_citygml_namespaces();
  ss << "<gml:name>my3dmap</gml:name>";
  for (auto& p3 : _lsFeatures) {
    ss << p3->get_citygml();
  }
  ss << "</CityModel>";
  return ss.str();
}

std::string Map3d::get_csv_buildings() {
  std::stringstream ss;
  ss << "id;roof;floor" << std::endl;
  for (auto& p : _lsFeatures) {
    if (p->get_class() == BUILDING) {
      Building* b = dynamic_cast<Building*>(p);
      // if (b != nullptr)
      ss << b->get_csv();
    }
  }
  return ss.str();
}

std::string Map3d::get_obj_per_feature(int z_exaggeration) {
  std::vector<int> offsets;
  offsets.push_back(0);
  std::stringstream ss;
  ss << "mtllib ./3dfier.mtl" << std::endl;
  for (auto& p3 : _lsFeatures) {
    ss << p3->get_obj_v(z_exaggeration);
    offsets.push_back(p3->get_number_vertices());
  }
  int i = 0;
  int offset = 0;
  for (auto& p3 : _lsFeatures) {
    ss << "o " << p3->get_id() << std::endl;
    offset += offsets[i++];
    ss << p3->get_obj_f(offset, true);
    if (_building_include_floor == true) {  
      Building* b = dynamic_cast<Building*>(p3);
      if (b != nullptr)
        ss << b->get_obj_f_floor(offset);
    }
 }
 return ss.str();
}

std::string Map3d::get_obj_per_class(int z_exaggeration) {
  std::vector<int> offsets;
  offsets.push_back(0);
  std::stringstream ss;
  ss << "mtllib ./3dfier.mtl" << std::endl;
  //-- go class by class sequentially
  for (int c = 0; c < 6; c++) {
    for (auto& p3 : _lsFeatures) {
      if (p3->get_class() == c) {
        ss << p3->get_obj_v(z_exaggeration);
        offsets.push_back(p3->get_number_vertices());
      }
    }
  }
  int i = 0;
  int offset = 0;
  for (int c = 0; c < 6; c++) {
    ss << "o " << c << std::endl;
    for (auto& p3 : _lsFeatures) {
      if (p3->get_class() == c) {
        offset += offsets[i++];
        ss << p3->get_obj_f(offset, false);
        if (_building_include_floor == true) {  
          Building* b = dynamic_cast<Building*>(p3);
          if (b != nullptr)
            ss << b->get_obj_f_floor(offset);
        }
      }
    }
  }
  return ss.str();
}

unsigned long Map3d::get_num_polygons() {
  return _lsFeatures.size();
}


const std::vector<TopoFeature*>& Map3d::get_polygons3d() {
  return _lsFeatures;
}


// void Map3d::add_elevation_point(double x, double y, double z, int returnno, liblas::Classification lasclass) {
void Map3d::add_elevation_point(liblas::Point const& laspt) {
  if ( (laspt.GetClassification() == liblas::Classification(1)) && (laspt.GetReturnNumber() != 1) )
    return;
  Point2 p(laspt.GetX(), laspt.GetY());
  std::vector<PairIndexed> re;
  // p.GetX(), p.GetY(), p.GetZ(), p.GetReturnNumber(), p.GetClassification()
  Point2 minp(laspt.GetX() - 0.1, laspt.GetY() - 0.1);
  Point2 maxp(laspt.GetX() + 0.1, laspt.GetY() + 0.1);
  Box2 querybox(minp, maxp);
  _rtree.query(bgi::intersects(querybox), std::back_inserter(re));
  for (auto& v : re) {
    TopoFeature* f = v.second;
    if (bg::distance(p, *(f->get_Polygon2())) < _radius_vertex_elevation) {     
      if (laspt.GetClassification() == liblas::Classification(1)) {
        if (f->get_class() == FOREST)
          f->add_elevation_point(laspt.GetX(), laspt.GetY(), laspt.GetZ(), _radius_vertex_elevation, false);
      }
      else
        f->add_elevation_point(laspt.GetX(), laspt.GetY(), laspt.GetZ(), _radius_vertex_elevation);
    }
  }
}


bool Map3d::threeDfy(bool triangulate) {
/*
  1. lift
  2. stitch
  3. CDT
*/
  std::clog << "===== /LIFTING =====" << std::endl;
  for (auto& p : _lsFeatures) {
    if (p->get_top_level() == true) {
      p->lift();
    }
    else
      std::clog << "niveau-1 " << p->get_id() << std::endl;
  }
  std::clog << "===== LIFTING/ =====" << std::endl;
  if (triangulate == true) {
    std::clog << "=====  /STITCHING =====" << std::endl;
    this->stitch_lifted_features();
    std::clog << "=====  STITCHING/ =====" << std::endl;
    std::clog << "=====  /CDT =====" << std::endl;
    for (auto& p : _lsFeatures) {
      // std::clog << p->get_id() << " (" << p->get_class() << ")" << std::endl;
      // if (p->get_id() == "107734797")
        // p->buildCDT();
      // else
        p->buildCDT();
    }
    std::clog << "=====  CDT/ =====" << std::endl;
  }
  return true;
}


bool Map3d::construct_rtree() {
  std::clog << "Constructing the R-tree...";
  for (auto p: _lsFeatures) 
    _rtree.insert(std::make_pair(p->get_bbox2d(), p));
  std::clog << " done." << std::endl;
  return true;
}


bool Map3d::add_polygons_file(std::string ifile, std::string idfield, std::vector< std::pair<std::string, std::string> > &layers) {
  std::clog << "Reading input dataset: " << ifile << std::endl;

#if GDAL_VERSION_MAJOR < 2
  if (OGRSFDriverRegistrar::GetRegistrar()->GetDriverCount() == 0) 
    OGRRegisterAll(); 
  OGRDataSource *dataSource = OGRSFDriverRegistrar::Open(ifile.c_str(), false);
#else
  if (GDALGetDriverCount() == 0) 
    GDALAllRegister();
  GDALDataset *dataSource = (GDALDataset*) GDALOpenEx(ifile.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
#endif

  if (dataSource == NULL) {
    std::cerr << "\tERROR: could not open file, skipping it." << std::endl;
    return false;
  }
  bool wentgood = this->extract_and_add_polygon(dataSource, idfield, layers);

  //-- find minx/miny of all datasets
  OGREnvelope bbox;
  for (auto l : layers) {
    OGRLayer *dataLayer = dataSource->GetLayerByName((l.first).c_str());
    if (dataLayer == NULL) {
      continue;
    }
    dataLayer->GetExtent(&bbox);
    if (bbox.MinX < bg::get<bg::min_corner, 0>(_bbox))
      bg::set<bg::min_corner, 0>(_bbox, bbox.MinX);
    if (bbox.MinY < bg::get<bg::min_corner, 1>(_bbox))
      bg::set<bg::min_corner, 1>(_bbox, bbox.MinY);
    if (bbox.MaxX > bg::get<bg::max_corner, 0>(_bbox))
      bg::set<bg::max_corner, 0>(_bbox, bbox.MaxX);
    if (bbox.MaxY > bg::get<bg::max_corner, 1>(_bbox))
      bg::set<bg::max_corner, 1>(_bbox, bbox.MaxY);
  }

  #if GDAL_VERSION_MAJOR < 2
    OGRDataSource::DestroyDataSource(dataSource);
  #else
    GDALClose(dataSource);
  #endif
  return wentgood;
}


bool Map3d::add_polygons_file(std::string ifile, std::string idfield, std::string lifting) {
#if GDAL_VERSION_MAJOR < 2
  if (OGRSFDriverRegistrar::GetRegistrar()->GetDriverCount() == 0) 
    OGRRegisterAll(); 
  OGRDataSource *dataSource = OGRSFDriverRegistrar::Open(ifile.c_str(), false);
#else
  if (GDALGetDriverCount() == 0) 
    GDALAllRegister();
  GDALDataset *dataSource = (GDALDataset*) GDALOpenEx(ifile.c_str(), GDAL_OF_READONLY, NULL, NULL, NULL);
#endif
  if (dataSource == NULL) {
    std::cerr << "\tERROR: could not open file, skipping it." << std::endl;
    return false;
  }
  std::vector< std::pair<std::string, std::string> > layers;
  int numberOfLayers = dataSource->GetLayerCount();
  for (int currentLayer = 0; currentLayer < numberOfLayers; currentLayer++) {
    OGRLayer *dataLayer = dataSource->GetLayer(currentLayer);
    std::pair<std::string, std::string> p(dataLayer->GetName(), lifting);
    layers.push_back(p);
  }
#if GDAL_VERSION_MAJOR < 2
  OGRDataSource::DestroyDataSource(dataSource);
#else
  GDALClose(dataSource);
#endif

  return add_polygons_file(ifile, idfield, layers);
}


#if GDAL_VERSION_MAJOR < 2
  bool Map3d::extract_and_add_polygon(OGRDataSource *dataSource, std::string idfield, std::vector< std::pair<std::string, std::string> > &layers)
#else
  bool Map3d::extract_and_add_polygon(GDALDataset *dataSource, std::string idfield, std::vector< std::pair<std::string, std::string> > &layers) 
#endif
{
  bool wentgood = true;
  for (auto l : layers) {
    OGRLayer *dataLayer = dataSource->GetLayerByName((l.first).c_str());
    if (dataLayer == NULL) {
      continue;
    }
    if (dataLayer->FindFieldIndex(idfield.c_str(), false) == -1) {
      std::cerr << "ERROR: field '" << idfield << "' not found." << std::endl;
      wentgood = false;
      continue;
    }
    dataLayer->ResetReading();
    unsigned int numberOfPolygons = dataLayer->GetFeatureCount(true);
    std::clog << "\tLayer: " << dataLayer->GetName() << std::endl;
    std::clog << "\t(" << numberOfPolygons << " features --> " << l.second << ")" << std::endl;
    OGRFeature *f;
    while ((f = dataLayer->GetNextFeature()) != NULL) {
      if ( (f->GetFieldIndex("hoogtenive") != -1) && (f->GetFieldAsInteger("hoogtenive") < 0) )
          continue;
      switch(f->GetGeometryRef()->getGeometryType()) {
        case wkbPolygon:
        case wkbMultiPolygon: 
        case wkbMultiPolygon25D:
        case wkbPolygon25D: {
          Polygon2* p2 = new Polygon2();
          // TODO : WKT surely not best/fastest way, to change. Or is it?
          char *wkt;
          f->GetGeometryRef()->flattenTo2D();
          f->GetGeometryRef()->exportToWkt(&wkt);
          bg::unique(*p2); //-- remove duplicate vertices
          if (l.second == "Building") {
            Building* p3 = new Building(wkt, f->GetFieldAsString(idfield.c_str()), _building_heightref_roof, _building_heightref_floor);
            _lsFeatures.push_back(p3);
          }
          else if (l.second == "Terrain") {
            Terrain* p3 = new Terrain(wkt, f->GetFieldAsString(idfield.c_str()), this->_terrain_simplification);
            _lsFeatures.push_back(p3);
          }
          else if (l.second == "Forest") {
            Forest* p3 = new Forest(wkt, f->GetFieldAsString(idfield.c_str()), this->_terrain_simplification);
            _lsFeatures.push_back(p3);
          }
          else if (l.second == "Water") {
            Water* p3 = new Water(wkt, f->GetFieldAsString(idfield.c_str()), this->_water_heightref);
            _lsFeatures.push_back(p3);
          }
          else if (l.second == "Road") {
            Road* p3 = new Road(wkt, f->GetFieldAsString(idfield.c_str()), this->_road_heightref);
            _lsFeatures.push_back(p3);
          }          
          //-- flag all polygons at (niveau < 0) for top10nl
          if ( (f->GetFieldIndex("hoogtenive") != -1) && (f->GetFieldAsInteger("hoogtenive") < 0) ) {
            // std::clog << "niveau=-1: " << f->GetFieldAsString(idfield.c_str()) << std::endl;
            (_lsFeatures.back())->set_top_level(false);
          }
          break;
        }
        default: {
          continue;
        }
      }
    }
  }
  return wentgood;
}


//-- http://www.liblas.org/tutorial/cpp.html#applying-filters-to-a-reader-to-extract-specified-classes
bool Map3d::add_las_file(std::string ifile, std::vector<int> lasomits, int skip) {
  std::clog << "Reading LAS/LAZ file: " << ifile << std::endl;
  if (lasomits.empty() == false) {
    std::clog << "\t(omitting LAS classes: ";
    for (int i : lasomits)
      std::clog << i << " ";
    std::clog << ")" << std::endl;
  }
  if ( (skip != 0) && (skip != 1) ) 
    std::clog << "\t(only reading every " << skip << "th points)" << std::endl;
  std::ifstream ifs;
  ifs.open(ifile.c_str(), std::ios::in | std::ios::binary);
  if (ifs.is_open() == false) {
    std::cerr << "\tERROR: could not open file, skipping it." << std::endl;
    return false;
  }
  //-- LAS classes to omit
  std::vector<liblas::Classification> liblasomits;
  for (int i : lasomits)
    liblasomits.push_back(liblas::Classification(i));
  //-- read each point 1-by-1
  liblas::ReaderFactory f;
  liblas::Reader reader = f.CreateWithStream(ifs);
  liblas::Header const& header = reader.GetHeader();
  std::clog << "\t(" << header.GetPointRecordsCount() << " points)" << std::endl;
  int i = 0;
  while (reader.ReadNextPoint()) {
    liblas::Point const& p = reader.GetPoint();
    if ( (skip != 0) && (skip != 1) ) {
      if (i % skip == 0) {
        if(std::find(liblasomits.begin(), liblasomits.end(), p.GetClassification()) == liblasomits.end())
          this->add_elevation_point(p);
      }
    }
    else {
      if(std::find(liblasomits.begin(), liblasomits.end(), p.GetClassification()) == liblasomits.end())
        this->add_elevation_point(p);
    }
    if (i % 100000 == 0) 
      printProgressBar(100 * (i / double(header.GetPointRecordsCount())));
    i++;
  }
  std::clog << "done" << std::endl;
  ifs.close();
  return true;
}


void Map3d::stitch_one_feature(TopoFeature* f, TopoClass adjclass) {
  std::vector<PairIndexed> re;
  _rtree.query(bgi::intersects(f->get_bbox2d()), std::back_inserter(re));
  for (auto& each : re) {
    // TopoFeature* fadj = each.second;
    if ( each.second->get_class() == adjclass ) {
      std::cout << each.second->get_id() << std::endl;
    }
  }
}


void Map3d::stitch_lifted_features() {
  for (auto& f : _lsFeatures) {
    std::vector<PairIndexed> re;
    _rtree.query(bgi::intersects(f->get_bbox2d()), std::back_inserter(re));
//-- 1. store all touching (adjacent + incident)
    std::vector<TopoFeature*> lstouching;
    for (auto& each : re) {
      TopoFeature* fadj = each.second;
      if (bg::touches(*(f->get_Polygon2()), *(fadj->get_Polygon2())) == true) {
        // std::cout << f->get_id() << "-" << f->get_class() << " : " << fadj->get_id() << "-" << fadj->get_class() << std::endl;
        lstouching.push_back(fadj);
      }
    }
//-- 2. build the node-column for each vertex
    // oring
    Ring2 oring = bg::exterior_ring(*(f->get_Polygon2())); 
    for (int i = 0; i < oring.size(); i++) {
      std::vector< std::pair<TopoFeature*, int> > star;  
      bool toprocess = true;
      for (auto& fadj : lstouching) {
        int index = fadj->has_point2(oring[i]);
        if (index != -1)  {
          if (f->get_counter() < fadj->get_counter()) {  //-- here that only lowID-->highID are processed
            star.push_back(std::make_pair(fadj, index));
          }
          else {
            toprocess = false;
            break;
          }
        }
      }
      if (toprocess == true) {
        this->process_star(f, i, star);
      }
    }
    // irings
    int offset = int(bg::num_points(oring));
    for (Ring2& iring: bg::interior_rings(*(f->get_Polygon2()))) {
      std::clog << f->get_id() << " irings " << std::endl;
      for (int i = 0; i < iring.size(); i++) {
        std::vector< std::pair<TopoFeature*, int> > star;  
        bool toprocess = true;
        for (auto& fadj : lstouching) {
          int index = fadj->has_point2(iring[i]);
          if (index != -1)  {
            if (f->get_counter() < fadj->get_counter()) {  //-- here that only lowID-->highID are processed
              star.push_back(std::make_pair(fadj, index));
            }
            else {
              toprocess = false;
              break;
            }
          }
        }
        if (toprocess == true) {
          this->process_star(f, (i + offset), star);
        }
      }
      offset += bg::num_points(iring);
    }
  }
}


void Map3d::process_star(TopoFeature* f, int pos, std::vector< std::pair<TopoFeature*, int> >& star) {
  float fz = f->get_point_elevation(pos);
  //-- degree of vertex == 2
  if (star.size() == 1) {
    //-- if same class, then average. TODO: always, also for water?
    if (f->get_class() == star[0].first->get_class())
        stitch_average(f, pos, star[0].first, star[0].second);
    else {
      if (f->is_hard() == true) {
        if (star[0].first->is_hard() == true)  {
          // TODO : order to check, vertical added to soft/2nd passed
          stitch_2_hard(f, pos, star[0].first, star[0].second, _threshold_jump_edges); 
        }
        else {
          stitch_jumpedge(f, pos, star[0].first, star[0].second, _threshold_jump_edges);
        }
      }
      else { //-- f is soft
        if (star[0].first->is_hard() == true)  {
          stitch_jumpedge(star[0].first, star[0].second, f, pos, _threshold_jump_edges);
        }
        else {
          stitch_average(f, pos, star[0].first, star[0].second);
        }
      }
    }
  }
  else if (star.size() > 1) {
    //-- collect all elevations
    std::vector<float> televs;
    std::vector<int> c;
    televs.assign(6, -999.0);
    c.assign(6, 0);
    televs[f->get_class()] = fz;
    for (auto& fadj : star) {
      if (c[fadj.first->get_class()] == 0) {
        televs[fadj.first->get_class()] = fadj.first->get_point_elevation(fadj.second);
        c[fadj.first->get_class()]++;
      }
      else {
        televs[fadj.first->get_class()] += fadj.first->get_point_elevation(fadj.second);
        c[fadj.first->get_class()]++;
      }
    }
    for (int i = 0; i < 6; i++) {
      if (c[i] != 0)
        televs[i] /= c[i];
    }
    if (f->get_id() == "111113638") {
      std::clog << televs[1] << std::endl;
    }
    adjust_nc(televs, _threshold_jump_edges);
    f->set_point_elevation(pos, televs[f->get_class()]);
    for (int i = 0; i <= 5; i++) {
      if ( (televs[i] > -998) && (i != f->get_class()) && (televs[i] < televs[f->get_class()]) )
        f->add_nc(pos, televs[i]);
    }

    for (auto& fadj : star) {
      // 1. set the elevation adjusted with the nc
      fadj.first->set_point_elevation(fadj.second, televs[fadj.first->get_class()]);
      // 2. if type ROAD/TERRAIN/FOREST && others are lower
      for (int i = 0; i <= 5; i++) {
        if ( (televs[i] > -998) && (i != fadj.first->get_class()) && (televs[i] < televs[fadj.first->get_class()]) )
          fadj.first->add_nc(fadj.second, televs[i]);
      }
    }
  }
}

// TODO : hard classes shouldn't be adjusted: can make unvertical water eg
void Map3d::adjust_nc(std::vector<float>& televs, float jumpedge) {
  for (int i = 1; i <= 5; i++) {
    for (int j = (i + 1); j <= 5; j++) {
      if ( (televs[i] >= -998) && (televs[j] >= -998) && (std::abs(televs[i] - televs[j]) < jumpedge) ) {
        televs[j] = televs[i];
      }
    }
  }
}


void Map3d::stitch_jumpedge(TopoFeature* hard, int hardpos, TopoFeature* soft, int softpos, float jumpedge) {
  float hardz = hard->get_point_elevation(hardpos);
  float deltaz = std::abs(hardz - soft->get_point_elevation(softpos));
  // std::clog << "deltaz=" << deltaz << std::endl;
  if (deltaz < jumpedge) 
    soft->set_point_elevation(softpos, hardz);
  else {
    soft->add_nc(softpos, hardz);
    // std::clog << "nc added: " << hardz << " (" << soft->get_point_elevation(softpos) << ")" << std::endl;
  }
}

void Map3d::stitch_average(TopoFeature* hard, int hardpos, TopoFeature* soft, int softpos) {
  float hardz = hard->get_point_elevation(hardpos);
  float softz = soft->get_point_elevation(softpos);
  hard->set_point_elevation(hardpos, (hardz + softz) / 2);
  soft->set_point_elevation(softpos, (hardz + softz) / 2);
}

void Map3d::stitch_2_hard(TopoFeature* hard, int hardpos, TopoFeature* soft, int softpos, float jumpedge) {
  // TODO: how to stitch 2 hard classes? Add VW for Road/Building?
  if (hard->get_class() == ROAD)
    hard->add_nc(hardpos, soft->get_point_elevation(softpos));
  else if (soft->get_class() == ROAD)
    soft->add_nc(softpos, hard->get_point_elevation(hardpos));
  else {
    std::clog << "STITCH_2_HARD: " << hard->get_class() << " --- " << soft->get_class() << std::endl;
    std::clog << "\t no ROAD involved, only WATER and BUILDING." << std::endl;
  }
}


bool Map3d::add_terrain_to_buildings() {
  std::stringstream ss;
  ss << "POLYGON((";
  ss << (bg::get<bg::min_corner, 0>(_bbox) - 100) << " ";
  ss << (bg::get<bg::min_corner, 1>(_bbox) - 100) << ", ";
  ss << (bg::get<bg::min_corner, 0>(_bbox) - 100) << " ";
  ss << (bg::get<bg::max_corner, 1>(_bbox) + 100) << ", ";
  ss << (bg::get<bg::max_corner, 0>(_bbox) + 100) << " ";
  ss << (bg::get<bg::max_corner, 1>(_bbox) + 100) << ", ";
  ss << (bg::get<bg::max_corner, 0>(_bbox) + 100) << " ";
  ss << (bg::get<bg::min_corner, 1>(_bbox) - 100) << ", ";
  ss << (bg::get<bg::min_corner, 0>(_bbox) - 100) << " ";
  ss << (bg::get<bg::min_corner, 1>(_bbox) - 100) << "))";
  Polygon2 se;
  bg::read_wkt(ss.str(), se);  

  std::clog << bg::area(se) << std::endl;

  
  return true;
}
