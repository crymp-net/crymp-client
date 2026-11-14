local function la(a,b)
	local b=b:gsub("%\\","/");
	LevelDesigner:AddShorten(a,b);
end
LevelDesigner.ShortenModels={
--Veh:
	["jet"]="vehicles/us_fighter_b/us_fighter";
--Lib:
	["alien_tower"]		="library/alien/level_specific_elements/maintenance/fog_lake_room/tower/tower_nocables";
	["ring"]		="library/alien/level_specific_elements/maintenance/thermal_towers/ring";
	["bartable"]		="library/furniture/tables/bartable_1";
	["wc"]			={"library/installations/sanitary/toilet","library/installations/sanitary/toiletseat","library/installations/sanitary/toiletseatcover","library/installations/sanitary/toilet_tank.cgf"};
	["divider"]		="library/installations/sanitary/divider";
	["door"]		="library/architecture/hillside_cafe/door_a";
	["roof"]		="library/architecture/village/roof/roofwithpoles_full";
	["pier"]		="library/architecture/harbour/pier/pier_wooden_detailed_1600x_rope";
	["cafe2"]		="library/architecture/village/cafe/cafe";
	["floor"]		="library/architecture/village/floor/floor_wooden_400_400_a";
	["lighthouse"]		="library/architecture/multiplayer/lighthouse/lighthouse";
	["container"]		="library/storage/crates/container/container_";		--red,green,blue
	["lamp"]		="library/architecture/harbour/light_system/harbour_streetlight";
	["reflectors"]		="library/architecture/harbour/light_system/harbour_floodlight";
	["excavator"]		="library/vehicles/excavator/excavator";
	["aeroplane"]		="library/vehicles/aircraft/aircraft";
	["ship"]		="library/vehicles/ship/roofed_rowing_boat/ship";	--_noroof,_trailer
	["crane"]		="library/vehicles/mobile_crane/mobile_crane";
	["fountain"]		="library/architecture/village/townhall_fauntain";
	["smallhut"]		="library/architecture/nodule/smallhut";
	["wall"]		="library/architecture/village/wall/concrete_wall_simple_16m_high";
	["house"]		="library/architecture/village/Village_house";
	["small_house"]		="library/architecture/village/house_small_";		--a ... b
	["tower"]		="library/architecture/watchtowers/watchtower_";	--asian,concrete
	["prototype"]		={"library/architecture/research_dome/research_dome_int","library/architecture/research_dome/research_dome_ext_mp"};
	["bridge"]		={"library/architecture/bridges/large_wired_bridge/large_bridge_start","library/architecture/bridges/large_wired_bridge/large_bridge_end"};
	["cafe"]		={
		"library/architecture/hillside_cafe/cafe_house",
		"library/architecture/hillside_cafe/terrace",
		"library/architecture/hillside_cafe/glass_01",
		"library/architecture/hillside_cafe/glass_02",
		"library/architecture/hillside_cafe/glass_03",
		"library/architecture/hillside_cafe/glass_04",
		"library/architecture/hillside_cafe/glass_05",
		"library/architecture/hillside_cafe/glass_06",
		"library/architecture/hillside_cafe/glass_07",
		"library/architecture/hillside_cafe/glass_08",
		"library/architecture/hillside_cafe/glass_09",
		"library/architecture/hillside_cafe/glass_10",
		"library/architecture/hillside_cafe/glass_11",
		"library/architecture/hillside_cafe/glass_12",
		"library/architecture/hillside_cafe/glass_13",
		"library/architecture/hillside_cafe/glass_14",
		"library/architecture/hillside_cafe/glass_15",
	};
--Natural:
	["palm"]		="natural/trees/palm_tree/palm_tree_large_";		--a ... h
	["iceberg"]		="natural/ice/iceberg_0";			--0 ... 8
	["snow"]		="natural/snow/snow";	
	["floe"]		="natural/ice/generic_floe_0";		--0 ... 8
	["crack_plate"]		="natural/ice/impact_area/crack_plate_big_";		--a ... f
	["bush"]		="natural/bushes/beachbush/beach_bush_";	--big/small_a...c
--ExtremeReplay pack, big thanks to Moridin:
	["checkpoint"]			="library/props/signs/freewaysignrack";
	["destroyedcontainer"]		="library/storage/crates/container/container_destroyed";
	["bigbush"]			="natural/bushes/junglebush/junglebush_big_";
	["big_bush"]			="natural/bushes/roundleafbush/round_leaf_bush_big_yellow";
	["brokencar"]			="library/vehicles/cars/car_b_chassi";
	["piergangway"]			="library/architecture/harbour/gangway/pier_gangway_";
	["fence"]			="library/barriers/fence/wooden/wooden_fence_120_200_a";
	["log"]				="natural/trees/jungle_tree_large/jungletree_saw";
	["guardhouse"]			="library/architecture/multiplayer/guardhouse/guardhouse";
	["personalbunker"]		="library/architecture/village/personal_bunker";
	["gateclosed"]			="library/barriers/boomgate/boomgate_a";
	["gate"]			="library/barriers/boomgate/boomgate_a_open";
	["sandbag"]			="library/barriers/sandbags/sandbag_big_structure_";
	["defensesign"]			="library/props/signs/auto_defense";
	["sign_militarywelcome"]	="library/props/signs/military_welcome";
	["explosivebarrel"]		="library/storage/barrels/barrel_explosive_red";
	["boxcover"]			="library/storage/civil/civil_box_b_cover_crouch";
	["platform"]			="library/architecture/mine/scaffolding/scaffolding_platform_4x2";
	["weaponrack"]			="library/architecture/multiplayer/probs/weaponsrack/weaponsrack";
	["footbridge"]			="library/architecture/railroads/footbridge";
	["ladder"]			="library/props/ladders/ladder_c";
	["wall"]			="natural/plants/rice_plants/wall_";
	["grass"]			="natural/ground_plants/grass/bigpatch_";
	["de_zone"]			="library/props/signs/de_zone";
	["office"]			="library/architecture/nodule/buildings/office_building";
	["orecrusher"]			="library/architecture/nodule/buildings/ore_processing_tower_crusher_mp";
	["forestpatch"]			="natural/ground_plants/forest_ground_patch_4m";
	["leaves"]			="natural/ground_plants/small_leaves/small_leaves_bunch_big_";
	["leaves_yellow"]		="natural/ground_plants/small_leaves/small_leaves_yellow_bunch_big_";
	["wire"]			="library/barriers/fence/wirefence_version2/wire_fence_b_";
	["brokenwire"]			="library/barriers/fence/wirefence_version2/wire_fence_b_4m_broken_";
	["forcefield"]			="library/alien/props/forcefield/forcefield_small";
	["asian_ice"]			="characters/human/asian/nk_soldier/nk_soldier_frozen_heavy_pose_";
	["caution"]			="library/props/signs/caution_sign";
	["sand_ramp"]			="natural/rocks/sand/sand_ramp_small_b";
	["jungle_rock"]			="natural/rocks/jungle_rocks/jungle_rock_";
	["forest_rock"]			="natural/rocks/forest_rocks/forest_rock_big_b_withgreen";
	["training_hut"]		="library/architecture/tutorial/combat_training_huts/combat_training_hut_";
	["roof_sandbag"]		="library/architecture/village/roof/roof_sandbag_a";
	["metal_roof"]			="library/architecture/village/roof/roof_metal_start_400_200_120";
	["sandbag_small"]		="library/barriers/sandbags/long_small_mg";
	["bridge_wooden"]		="library/architecture/bridges/small_wooden_bridge/small_wooden_bridge";
	["bridge_wooden_pillar"]	="library/architecture/bridges/small_wooden_bridge/small_wooden_bridge_pillar";
	["pier_end"]			="library/architecture/harbour/pier/pier_wooden_detailed_45end";
	["pier_start"]			="library/architecture/harbour/pier/pier_wooden_detailed_45start";
	["pier_800"]			="library/architecture/harbour/pier/pier_wooden_detailed_800x";
	["bush_cliff"]			="natural/bushes/cliffbush/cliff_bush_green_mini";
	["palm_bush"]			="natural/bushes/palm_bush/palmbush_big_";
	["bush_red"]			="natural/bushes/junglebush/junglebush_big_fl_red_a";
	["bush_pink"]			="natural/bushes/junglebush/junglebush_big_fl_pink_a";
	["bridge_plank"]		="library/props/building material/wooden_shelves";
	["fern_green"]			="natural/bushes/greenfernbush/green_fern_bush_";
	["jungle_tree"]			="natural/trees/jungle_tree_thin/jungle_tree_thin_g";
	["tree_twisted"]		="natural/trees/twisted_tree/twisted_tree_";
	["tree_twistedground"]		="natural/trees/twisted_tree/twisted_tree_ground_a";
	["graveyard_wall2m"]		="library/architecture/toomb/graveyard_wall_2m";
	["graveyard_wall4m"]		="library/architecture/toomb/graveyard_wall_4m";
	["graveyard_wall8m"]		="library/architecture/toomb/graveyard_wall_8m";
	["graveyard_corner"]		="library/architecture/toomb/graveyard_wall_corner";
	["graveyard_end"]		="library/architecture/toomb/graveyard_wall_endpiece";
	["graveyard_corner"]		="library/architecture/toomb/graveyard_wall_t_corner";
	["graveyard_tomb"]		="library/architecture/toomb/toomb_small_";
	["graveyard_cross"]		="library/architecture/toomb/kross_";
	["graveyard_tombbig"]		="library/architecture/toomb/toomb_big_a";
	["graveyard_tombopen"]		="library/architecture/toomb/toomb_med_open_b";
	["tree_dead"]			="natural/trees/jungle_tree_thin/jungle_tree_thin_dead_a";
	["glasstube"]			="library/alien/level_specific_elements/maintenance/fog_lake_room/machines/glass_tube";
	["tree_dead_small"]		="natural/trees/twisted_tree/twisted_tree_dead_small_a";
	["sign_both"]			="library/props/signs/street_sign_k";
	["sign_right"]			="library/props/signs/street_sign_n";
	["sign_left"]			="library/props/signs/street_sign_n_b";
	["generator"]			="library/architecture/mobile_camp_structures/mobile_generator_chinese/mobile_generator";
	["catwalk"]			="library/barriers/concrete_wall/wall_catwalk_800x";
	["catwalk_corner"]		="library/barriers/concrete_wall/wall_catwalk_corner";
	["catwalk_stairs"]		="library/barriers/concrete_wall/wall_catwalk_800x_stairs";
	["catwalkmg"]			="library/barriers/concrete_wall/wall_catwalk_800x_mg";
	["catwalk_tcorner"]		="library/barriers/concrete_wall/wall_catwalk_tcorner";
	["catwalk_end"]			="library/barriers/concrete_wall/wall_catwalk_end";
	["catwalk_endopen"]		="library/barriers/concrete_wall/wall_catwalk_top_endopen";
	["village_school"]		="library/architecture/village/school";
	["village_house_broken"]	="library/architecture/village/village_house_4_broken_a_statik";
	["plane_broken"]		="library/architecture/airfield/crashed_us_cargoplane/us_cargoplane_destroyed";
	["shiten_tripod"]		="weapons/asian/shi_ten/tripod_tp";
	["watertower"]			="library/architecture/watchtowers/water_tower";
	["watertower_ladder"]		="library/architecture/watchtowers/water_tower_ladder";
	["tower_broken"]		="library/architecture/watchtowers/watchtower_concreteb_destroyed";
	["container_crane"]		="library/machines/cranes/container_crane/container_crane";
	["table"]			="library/furniture/tables/table_asian_breakable";
	["grasspatch"]		={
		"natural/ground_plants/marsh_grass/marshgrass_6m_patch_a",
		"natural/ground_plants/marsh_grass/marshgrass_6m_patch_b",
	};
	["parkplace"]		={
		"library/architecture/airfield/parking_lot/parkplace",
		"library/architecture/airfield/parking_lot/parkplace_concrete_10x10m",
	};
	["junglepatch"]		={
		"natural/plants/jungle_grass/junglegrass_patch_a",
		"natural/plants/jungle_grass/junglegrass_patch_b",
		"natural/plants/jungle_grass/junglegrass_patch_c",
	};
	["junglebush"]		={
		"natural/bushes/junglebush/junglebush_big_fl_red_a",
		"natural/bushes/junglebush/junglebush_big_b",
	};
--tunnels
	["tunnel_grate"]		="library/alien/level_specific_elements/maintenance/shafts/shaft_breaking_grate_tunnel";
	["tunnel_curve"]		="library/alien/level_specific_elements/maintenance/shafts/shaft_curve";
	["tunnel_curve_up"]		="library/alien/level_specific_elements/maintenance/shafts/shaft_curve_up";
	["tunnel_slope_down"]	="library/alien/level_specific_elements/maintenance/shafts/shaft_slope_down";
	["tunnel_slope_up"]		="library/alien/level_specific_elements/maintenance/shafts/shaft_slope_up";
	["tunnel_straight"]		="library/alien/level_specific_elements/maintenance/shafts/shaft_straight";
--concrete walls
	["concretewall_16m"]		="library/barriers/concrete_wall/concrete_wall_base_16m";
	["concretewall_broken"]		="library/barriers/concrete_wall/concrete_wall_base_16m_destroyedc";
	["concretewall_door"]		="library/barriers/concrete_wall/concrete_wall_base_8m_door";
	["concretewall_window"]		="library/barriers/concrete_wall/concrete_wall_base_4m_window";
	["concretewall_corner"]		="library/barriers/concrete_wall/concrete_wall_base_corner45_out";
--jungle trees
	["jungletree_biggreen"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green";
	["jungletree_fallen"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green_fallen_";
	["jungletree_leaning"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green_leaning";
	["jungletree_greygreen"]	="natural/trees/jungle_tree_large/jungle_tree_large_big_grey_green";
	["jungletree_biggreen_120"]	="natural/trees/jungle_tree_large/jungle_tree_large_big_grey_green_120_deg";
	["jungletree_biggreen_240"]	="natural/trees/jungle_tree_large/jungle_tree_large_big_grey_green_240_deg";
	["jungletree_biggreen_360"]	="natural/trees/jungle_tree_large/jungle_tree_large_big_grey_green_360_deg";
	["jungletree_bigyellow"]	="natural/trees/jungle_tree_large/jungle_tree_large_big_yellow";
	["jungletree_medgreen"]		="natural/trees/jungle_tree_large/jungle_tree_large_med_bright_green";
	["jungletree_noleaves"]		="natural/trees/jungle_tree_large/jungle_tree_large_med_noleaves_a";
	["jungletree_medyellow"]	="natural/trees/jungle_tree_large/jungle_tree_large_med_yellow";
	["jungletree_smallgreen"]	="natural/trees/jungle_tree_large/jungle_tree_large_small_bright_green";
	["jungletree_smallyellow"]	="natural/trees/jungle_tree_large/jungle_tree_large_small_yellow";
	["jungletree_fallen"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green_fallen_b";
	["jungletree_fallen2"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green_fallen_c";
	["jungletree_fallen3"]		="natural/trees/jungle_tree_large/jungle_tree_large_big_bright_green_fallen_e";
	["jungletree_biglog"]		="natural/trees/jungle_tree_large/saw_tree_a";
--banana trees
	["bananatree_biga"]		="natural/trees/banana_tree/bananatree_big_a";
	["bananatree_bigb"]		="natural/trees/banana_tree/bananatree_big_b";
	["bananatree_old"]		="natural/trees/banana_tree/bananatree_old_b";
	["bananas"]				="natural/trees/banana_tree/bananas_only_a";
--spawn bunker
	["spawnbunker_ceiling1"]	="library/architecture/multiplayer/spawn_bunker/sb_ceiling01";
	["spawnbunker_ceiling2"]	="library/architecture/multiplayer/spawn_bunker/sb_ceiling02";
	["spawnbunker_ceiling3"]	="library/architecture/multiplayer/spawn_bunker/sb_ceiling03";
	["spawnbunker_floor1"]		="library/architecture/multiplayer/spawn_bunker/sb_floor01";
	["spawnbunker_floor2"]		="library/architecture/multiplayer/spawn_bunker/sb_floor02";
	["spawnbunker_floor3"]		="library/architecture/multiplayer/spawn_bunker/sb_floor03";
	["spawnbunker_wall1"]		="library/architecture/multiplayer/spawn_bunker/sb_wall01";
	["spawnbunker_wall2"]		="library/architecture/multiplayer/spawn_bunker/sb_wall02";
	["spawnbunker_wall3"]		="library/architecture/multiplayer/spawn_bunker/sb_wall03";
	["spawnbunker_wall4"]		="library/architecture/multiplayer/spawn_bunker/sb_wall04";
	["spawnbunker_wall5"]		="library/architecture/multiplayer/spawn_bunker/sb_wall05";
	["spawnbunker_wall6"]		="library/architecture/multiplayer/spawn_bunker/sb_wall06";
	["spawnbunker_wall7"]		="library/architecture/multiplayer/spawn_bunker/sb_wall07";
	["spawnbunker_wall8"]		="library/architecture/multiplayer/spawn_bunker/sb_wall08";
	["spawnbunker_wall9"]		="library/architecture/multiplayer/spawn_bunker/sb_wall09";
	["spawnbunker_wall10"]		="library/architecture/multiplayer/spawn_bunker/sb_wall10";
	["spawnbunker"]				="library/architecture/multiplayer/spawn_bunker/spawn_bunker";
	["spawnbunker_stairs"]		="library/architecture/multiplayer/spawn_bunker/spawnbunker_stair";
};
la("box",[[Objects\box.cgf]]);	--use la(shorten,full) or LevelDesigner:AddShorten(shorten,full) to add model