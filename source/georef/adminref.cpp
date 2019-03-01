/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#include "adminref.h"
#include <boost/algorithm/string/join.hpp>
#include <boost/geometry.hpp>

namespace navitia { namespace georef {
std::string Admin::get_range_postal_codes(){
    std::string post_code;
    // the label is the range of the postcode
    // ex: Tours (37000;37100;37200) -> Tours (37000-37200)
    //     Cosne-d'Allier (03430)    -> Cosne-d'Allier (03430)
    if (!this->postal_codes.empty()) {
        try {
            std::vector<int> postal_codes_int;
            for (const std::string& str_post_code : this->postal_codes) {
                postal_codes_int.push_back(boost::lexical_cast<int>(str_post_code));
            }
            auto result = std::minmax_element(postal_codes_int.begin(),postal_codes_int.end());
            if (*result.first == *result.second){
                post_code = this->postal_codes[result.first-postal_codes_int.begin()];
            }else{
                post_code = this->postal_codes[result.first-postal_codes_int.begin()]
                        + "-" + this->postal_codes[result.second-postal_codes_int.begin()];
            }
        }catch (boost::bad_lexical_cast) {
            post_code = this->postal_codes_to_string();
        }
    }
    return post_code;
}

std::string Admin::postal_codes_to_string() const{
    return boost::algorithm::join(this->postal_codes, ";");
}


AdminRtree build_admins_tree(const std::vector<Admin*> admins) {
    AdminRtree admins_tree;
    for(auto* admin: admins){
        add_admin_in_tree(admins_tree, admin);
    }
    return admins_tree;
}

void add_admin_in_tree(AdminRtree& rtree, Admin* admin) {
    double min[2];
    double max[2];
    boost::geometry::model::box<nt::GeographicalCoord> box;
    boost::geometry::envelope(admin->boundary, box);
    min[0] = box.min_corner().lon();
    min[1] = box.min_corner().lat();
    max[0] = box.max_corner().lon();
    max[1] = box.max_corner().lat();
    rtree.Insert(min, max, admin);
}

std::vector<Admin*> find_admins_in_tree(AdminRtree& rtree, const type::GeographicalCoord& coord) {
    std::vector<Admin*> result;

    auto callback = [](Admin* admin, void* c)->bool{
        auto* context = reinterpret_cast<std::pair<type::GeographicalCoord, std::vector<Admin*>*>*>(c);
        if(boost::geometry::within(context->first, admin->boundary)){
            context->second->push_back(admin);
        }
        return true;
    };
    double c[2];
    c[0] = coord.lon();
    c[1] = coord.lat();
    auto context = std::make_pair(coord, &result);
    rtree.Search(c, c, callback, &context);
    return result;
}
}}
