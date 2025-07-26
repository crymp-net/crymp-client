#include "WeaponSystem.h"

void CWeaponSystem::RegisterXMLData()
{
	// Scripts/Entities/Items/XML/Ammo/50bullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "50bullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.04));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(900));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "bullet.hit_incendiary.heli50");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(0.5));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("50bullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/A2AHomingMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "a2ahomingmissile");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(16));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1800));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(220));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "autoControlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "detonation_radius");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "initial_delay");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.sidewinder");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("a2ahomingmissile", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/A2AHomingMissile_AscMod.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "a2ahomingmissile_ascmod");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(75));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1800));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.6));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(250));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "autoControlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "detonation_radius");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "initial_delay");
				node_params_param->SetAttribute("value", static_cast<float>(0.0));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.sidewinder");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("a2ahomingmissile_ascmod", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/A2GHomingMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "a2ghomingmissile");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(16));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1800));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.6));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "accel");
				node_params_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_speed");
				node_params_param->SetAttribute("value", static_cast<int>(40));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "align_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "descend_distance");
				node_params_param->SetAttribute("value", static_cast<int>(100));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.sidewinder");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("a2ghomingmissile", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "acmo");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.225));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "Alien_Weapons.Moac.Scout_Moac_Impact");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.Moac.Tracer_scout");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("acmo", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/AVExplosive.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "avexplosive");
		node->SetAttribute("class", "AVExplosive");
		node->SetAttribute("script", "scripts/entities/items/explosive.lua");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/at_mine/at_mine.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/at_mine/at_mine_tp.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "static");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "avmine");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "triggerweight");
				node_params_param->SetAttribute("value", static_cast<int>(250));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "box_dimension");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "hitPoints");
				node_params_param->SetAttribute("value", static_cast<int>(300));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "quietRemoval");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
		}

		this->RegisterAmmo("avexplosive", "AVExplosive", "scripts/entities/items/explosive.lua", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Bullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "bullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.0125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1100));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("bullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/BunkerBuster.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "bunkerbuster");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(40));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.FFAR");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("bunkerbuster", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/C4Explosive.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "c4explosive");
		node->SetAttribute("class", "C4Projectile");
		node->SetAttribute("script", "scripts/entities/items/explosive.lua");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/c4/c4_tp.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/c4/c4_tp.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "rigid");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(12));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(65));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "c4");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "hitPoints");
				node_params_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "quietRemoval");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "noBulletHits");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("c4explosive", "C4Projectile", "scripts/entities/items/explosive.lua", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ClaymoreExplosive.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "claymoreexplosive");
		node->SetAttribute("class", "ClaymoreExplosive");
		node->SetAttribute("script", "scripts/entities/items/explosive.lua");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/claymore/claymore.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/claymore/claymore.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "static");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "trigger_radius");
				node_params_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "trigger_angle");
				node_params_param->SetAttribute("value", static_cast<int>(60));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "arm_delay");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "hitPoints");
				node_params_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "quietRemoval");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
		}

		this->RegisterAmmo("claymoreexplosive", "ClaymoreExplosive", "scripts/entities/items/explosive.lua", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ExplosiveGrenade.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "explosivegrenade");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}

		this->RegisterAmmo("explosivegrenade", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ExplosiveGrenadeAI.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "explosivegrenadeAI");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}

		this->RegisterAmmo("explosivegrenadeAI", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/FYBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "fybullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.035));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(710));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("fybullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Flashbang.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "flashbang");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.flashbang.explode");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_flashbang = node->InsertChild("flashbang");
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "max_radius");
				node_flashbang_param->SetAttribute("value", static_cast<float>(25.0));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "blindAmount");
				node_flashbang_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "flashbangBaseTime");
				node_flashbang_param->SetAttribute("value", static_cast<float>(2.5));
			}
		}

		this->RegisterAmmo("flashbang", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/FlashbangAI.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "FlashbangAI");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(26));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.flashbang.explode");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(1.8));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_flashbang = node->InsertChild("flashbang");
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "max_radius");
				node_flashbang_param->SetAttribute("value", static_cast<float>(25.0));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "blindAmount");
				node_flashbang_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "flashbangBaseTime");
				node_flashbang_param->SetAttribute("value", static_cast<float>(2.5));
			}
		}

		this->RegisterAmmo("FlashbangAI", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/GaussBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "gaussbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:shrapnel_by");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.075));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(10000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "explosions.gauss.hit");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
		}

		this->RegisterAmmo("gaussbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/HeavyACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "heavyacmo");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/fx/moac_trail.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/fx/moac_trail.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(80));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "Alien_Weapons.Moac.Scout_Moac_Impact");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(0.75));
			}
		}

		this->RegisterAmmo("heavyacmo", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/HomingMOAC.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "homingmoac");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(75));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.035));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(280));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.075));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_Rocket.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.45));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("homingmoac", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/HomingMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "homingmissile");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(18));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(16));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.6));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "accel");
				node_params_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_speed");
				node_params_param->SetAttribute("value", static_cast<int>(120));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(80));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "align_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "descend_distance");
				node_params_param->SetAttribute("value", static_cast<int>(75));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_Rocket.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("homingmissile", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/HurricaneBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "hurricanebullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(25));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.025));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(4));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("hurricanebullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/IncendiaryBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "incendiarybullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:incendiary_bullet_whiz");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.02));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(710));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "bullet.hit_incendiary.a");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
		}

		this->RegisterAmmo("incendiarybullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/LightACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "lightacmo");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(28));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.Moac.Tracer");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("lightacmo", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/LightBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "lightbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.06));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(360));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("lightbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/MGbullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "MGbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.04));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(900));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("MGbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/ExplosiveGrenade.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "explosivegrenade");
		node->SetAttribute("class", "Projectile");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}

		this->RegisterAmmo("explosivegrenade", "Projectile", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Flashbang.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "flashbang");
		node->SetAttribute("class", "Projectile");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.flashbang.explode");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_flashbang = node->InsertChild("flashbang");
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "max_radius");
				node_flashbang_param->SetAttribute("value", static_cast<float>(20.0));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "blindAmount");
				node_flashbang_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_flashbang_param = node_flashbang->InsertChild("param");
				node_flashbang_param->SetAttribute("name", "flashbangBaseTime");
				node_flashbang_param->SetAttribute("value", static_cast<int>(5));
			}
		}

		this->RegisterAmmo("flashbang", "Projectile", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Rocket.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "rocket");
		node->SetAttribute("class", "HomingMissile");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1700));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "law_rocket");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.035));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(240));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.6));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_Rocket.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.45));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("rocket", "HomingMissile", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/SCARGrenade.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "scargrenade");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/shells/scar_gl_grenade.cgf");
				node_geometry_firstperson->SetAttribute("scale", static_cast<int>(4));
				node_geometry_firstperson->SetAttribute("angles", Vec3(0,0,-90));
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/shells/scar_gl_grenade.cgf");
				node_geometry_thirdperson->SetAttribute("scale", static_cast<int>(6));
				node_geometry_thirdperson->SetAttribute("angles", Vec3(0,0,-90));
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(45));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(1440, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("scargrenade", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/DumbAAMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "dumbaamissile");
		node->SetAttribute("class", "HomingMissile");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(280));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.075));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.AA");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("dumbaamissile", "HomingMissile", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/ExocetMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "exocetmissile");
		node->SetAttribute("class", "HomingMissile");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(16));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1600));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.exocet");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "accel");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(120));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_speed");
				node_params_param->SetAttribute("value", static_cast<int>(60));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "align_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "descend_distance");
				node_params_param->SetAttribute("value", static_cast<int>(750));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.exocet");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("exocetmissile", "HomingMissile", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/GaussTankBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "gausstankbullet");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(10000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.81));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "explosions.gauss.tank_gauss");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.gauss");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("gausstankbullet", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/HelicopterMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "helicoptermissile");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1600));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.FFAR");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("helicoptermissile", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/TACProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tacprojectile");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(110));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.75));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(300));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(55));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.tac");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tacprojectile", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/TOWMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "towmissile");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1800));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.tow");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("towmissile", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/Tank125.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tank125");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(350));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.81));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1600));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.35));
			}
		}

		this->RegisterAmmo("tank125", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/Tank30.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tank30");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.tank30.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.apc30");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tank30", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/TankAA.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tankaa");
		node->SetAttribute("class", "Bullet");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(0));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.0125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(800));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.AA_round.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tankaa", "Bullet", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/VTOL20.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "vtol20");
		node->SetAttribute("class", "Rocket");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(300));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.9));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.tank30.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.apc30");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("vtol20", "Rocket", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/Vehicles/VTOLBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "vtolbullet");
		node->SetAttribute("class", "Bullet");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_large");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(25));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.0065));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(800));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "bullet.hit_incendiary.hurricane_vtol");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("vtolbullet", "Bullet", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Multiplayer/alienmount_ACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "alienmount_acmo");
		node->SetAttribute("class", "Bullet");
		node->SetAttribute("configuration", "mp");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(1.0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(180));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "Alien_Weapons.Moac.Mount_Moac_Impact");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.Moac.Tracer_alienmount");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}

		this->RegisterAmmo("alienmount_acmo", "Bullet", "", "mp", node);
	}
	// Scripts/Entities/Items/XML/Ammo/NanoDisruptor.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "empgrenade");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/emp_grenade/emp_grenade.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/emp_grenade/emp_grenade.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_blue");
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "misc.emp.sphere");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "emp");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
		}

		this->RegisterAmmo("empgrenade", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Rock.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "rock");
		node->SetAttribute("class", "Rock");
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "rigid");
		}

		this->RegisterAmmo("rock", "Rock", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Rocket.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "rocket");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(75));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1700));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.035));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(240));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.6));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_Rocket.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.45));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("rocket", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/RubberBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "rubberbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.00125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(800));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("rubberbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/SCARGrenade.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "scargrenade");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/shells/scar_gl_grenade.cgf");
				node_geometry_firstperson->SetAttribute("scale", static_cast<int>(4));
				node_geometry_firstperson->SetAttribute("angles", Vec3(0,0,-90));
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/shells/scar_gl_grenade.cgf");
				node_geometry_thirdperson->SetAttribute("scale", static_cast<int>(6));
				node_geometry_thirdperson->SetAttribute("angles", Vec3(0,0,-90));
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(60));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(1440, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "muzzleflash.LAM.grenade_white");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("scargrenade", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/SMGBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "smgbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.025));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(725));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("smgbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ScoutSingularityProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "scoutsingularityprojectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(500));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "Alien_Weapons.singularity.Scout_Singularity_Impact");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.singularity.Scout_Singularity_Projectile");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "Sounds/weapons:singularity_cannon:sing_cannon_flying_loop");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound_radius");
				node_trail_param->SetAttribute("value", static_cast<int>(40));
			}
		}

		this->RegisterAmmo("scoutsingularityprojectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/ShotgunShell.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "shotgunshell");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.06));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(460));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("shotgunshell", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/SingularityProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "singularityprojectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "Alien_Weapons.singularity.Hunter_Singularity_Impact");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.singularity.Hunter_Singularity_Projectile");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.6));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "Sounds/weapons:singularity_cannon:sing_cannon_flying_loop");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound_radius");
				node_trail_param->SetAttribute("value", static_cast<int>(50));
			}
		}

		this->RegisterAmmo("singularityprojectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/SmokeGrenade.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "smokegrenade");
		node->SetAttribute("class", "Projectile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/flashbang/flashbang.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(22));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_grenade");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "spin_random");
				node_physics_param->SetAttribute("value", Vec3(720, 720, 720));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(35.0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.05));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "grenade");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "explosions.Smoke_grenade.smoke");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "prime");
				node_trail_param->SetAttribute("value", false);
			}
		}
		{
			IItemParamsNode* node_scaledeffect = node->InsertChild("scaledeffect");
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "postEffect");
				node_scaledeffect_param->SetAttribute("value", "FilterBlurring_Amount");
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "maxvalue");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "radius");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "delay");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "fadeintime");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "fadeouttime");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_scaledeffect_param = node_scaledeffect->InsertChild("param");
				node_scaledeffect_param->SetAttribute("name", "AIObstructionRadius");
				node_scaledeffect_param->SetAttribute("value", static_cast<int>(5));
			}
		}

		this->RegisterAmmo("smokegrenade", "Projectile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/SniperBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "sniperbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_ricochet = node->InsertChild("ricochet");
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "sound");
				node_ricochet_param->SetAttribute("value", "sounds/physics:bullet_ricochet:bullet_ricochet");
			}
			{
				IItemParamsNode* node_ricochet_param = node_ricochet->InsertChild("param");
				node_ricochet_param->SetAttribute("name", "speed");
				node_ricochet_param->SetAttribute("value", static_cast<int>(16));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.025));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "bulletType");
				node_params_param->SetAttribute("value", static_cast<int>(3));
			}
		}
		{
			IItemParamsNode* node_trailUnderWater = node->InsertChild("trailUnderWater");
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "effect");
				node_trailUnderWater_param->SetAttribute("value", "water.bubbles.bullets");
			}
			{
				IItemParamsNode* node_trailUnderWater_param = node_trailUnderWater->InsertChild("param");
				node_trailUnderWater_param->SetAttribute("name", "scale");
				node_trailUnderWater_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("sniperbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/TACGunProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tacgunprojectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/us/frag_grenade/frag_grenade_tp_inair.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.55));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(40));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(55));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.tac");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.75));
			}
		}

		this->RegisterAmmo("tacgunprojectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/TACGunProjectileFleet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tacgunprojectileEx");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.55));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(280));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "autoControlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "initial_delay");
				node_params_param->SetAttribute("value", static_cast<float>(0.4));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.sidewinder");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("tacgunprojectileEx", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/TacBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tacbullet");
		node->SetAttribute("class", "TacBullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.00225));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(250));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "sleepTime");
				node_params_param->SetAttribute("value", static_cast<float>(60.0));
			}
		}

		this->RegisterAmmo("tacbullet", "TacBullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/TagBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tagbullet");
		node->SetAttribute("class", "TagBullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_small");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(22));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.00025));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(600));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("tagbullet", "TagBullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/TurretACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "turretacmo");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.225));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "Alien_Weapons.Moac.Warrior_Turret_Impact");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(0.33));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.Moac.Tracer_warrior");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(3.0));
			}
		}

		this->RegisterAmmo("turretacmo", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Turret_Rocket.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "turret_rocket");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(3.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(45));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(280));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(500));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_Rocket.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.45));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("turret_rocket", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/DumbAAMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "dumbaamissile");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise");
				node_params_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "controlled");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(280));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_target_distance");
				node_params_param->SetAttribute("value", static_cast<int>(2000));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lazyness");
				node_params_param->SetAttribute("value", static_cast<float>(0.075));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.AA");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("dumbaamissile", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/ExocetMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "exocetmissile");
		node->SetAttribute("class", "HomingMissile");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(100));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(16));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.rocket_terrain.exocet");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.0));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "accel");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "turn_speed");
				node_params_param->SetAttribute("value", static_cast<int>(120));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "max_speed");
				node_params_param->SetAttribute("value", static_cast<int>(60));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "cruise_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "align_altitude");
				node_params_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "descend_distance");
				node_params_param->SetAttribute("value", static_cast<int>(750));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "detonation_radius");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "ignoreWater");
				node_params_param->SetAttribute("value", true);
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.exocet");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "sounds/physics:bullet_whiz:missile_whiz_loop");
			}
		}

		this->RegisterAmmo("exocetmissile", "HomingMissile", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/GaussTankBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "gausstankbullet");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(10000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.81));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "explosions.gauss.tank_gauss");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.gauss");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("gausstankbullet", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/HelicopterMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "helicoptermissile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.weapon_stinger.FFAR");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("helicoptermissile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/TACProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tacprojectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(110));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.3));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.75));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "traceable");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(300));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(140));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.7));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(55));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.tac");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tacprojectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/TOWMissile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "towmissile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(150));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1800));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(10));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "aitype");
				node_params_param->SetAttribute("value", "rpg");
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.tow");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("towmissile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/Tank125.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tank125");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.35));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(350));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -9.81));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1600));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "decal");
				node_explosion_param->SetAttribute("value", "textures/decal/explo_decal.dds");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.35));
			}
		}

		this->RegisterAmmo("tank125", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/Tank30.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tank30");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/effects/tracer_standard_new.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/effects/tracer_standard_new.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.tank30.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.apc30");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tank30", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/TankAA.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "tankaa");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(0));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.0125));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(800));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<float>(2.5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.AA_round.Trail");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("tankaa", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/Tank_SingularityProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "Tank_singularityprojectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(13));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(20));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "tac");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.singularity.Tank_Singularity_Projectile");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.6));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "Sounds/weapons:singularity_cannon:sing_cannon_flying_loop");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound_radius");
				node_trail_param->SetAttribute("value", static_cast<int>(50));
			}
		}

		this->RegisterAmmo("Tank_singularityprojectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/VTOL20.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "vtol20");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/effects/tracer_standard_red.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(1200));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.2));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<float>(0.5));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(300));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.0));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.9));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<float>(1.5));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(4));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "explosions.tank30.explosion");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "type");
				node_explosion_param->SetAttribute("value", "frag");
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(5));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "smoke_and_fire.Tank_round.apc30");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<int>(1));
			}
		}

		this->RegisterAmmo("vtol20", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/Vehicles/VTOLBullet.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "vtolbullet");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:bullet_whiz_large");
			}
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "speed");
				node_whiz_param->SetAttribute("value", static_cast<int>(25));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(0.0065));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(800));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, -1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "bullet.hit_incendiary.hurricane_vtol");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(2));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.15));
			}
		}

		this->RegisterAmmo("vtolbullet", "Bullet", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/WarriorSingularityProjectile.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "WarriorSingularityProjectile");
		node->SetAttribute("class", "Rocket");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/rockets/sidewinder/sidewinder.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "serverspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "predictspawn");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<int>(1000));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "radius");
				node_physics_param->SetAttribute("value", static_cast<float>(0.25));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "thrust");
				node_physics_param->SetAttribute("value", static_cast<int>(50));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "lift");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "min_bounce_speed");
				node_physics_param->SetAttribute("value", static_cast<int>(8));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "material");
				node_physics_param->SetAttribute("value", "mat_metal");
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(9));
			}
		}
		{
			IItemParamsNode* node_explosion = node->InsertChild("explosion");
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "pressure");
				node_explosion_param->SetAttribute("value", static_cast<int>(1500));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(15));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "min_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<float>(2.2));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "max_phys_radius");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(3));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "terrain_hole_size");
				node_explosion_param->SetAttribute("value", static_cast<int>(6));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect");
				node_explosion_param->SetAttribute("value", "Alien_Weapons.singularity.Warrior_Singularity_Impact");
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "effect_scale");
				node_explosion_param->SetAttribute("value", static_cast<float>(0.8));
			}
			{
				IItemParamsNode* node_explosion_param = node_explosion->InsertChild("param");
				node_explosion_param->SetAttribute("name", "radialblurdist");
				node_explosion_param->SetAttribute("value", static_cast<int>(30));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(30));
			}
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "showtime");
				node_params_param->SetAttribute("value", static_cast<float>(0.1));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.singularity.Warrior_Singularity_Projectile");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(0.6));
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound");
				node_trail_param->SetAttribute("value", "Sounds/weapons:singularity_cannon:sing_cannon_flying_loop");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "sound_radius");
				node_trail_param->SetAttribute("value", static_cast<int>(50));
			}
		}

		this->RegisterAmmo("WarriorSingularityProjectile", "Rocket", "", "", node);
	}
	// Scripts/Entities/Items/XML/Ammo/alienmount_ACMO.xml
	{
		IItemParamsNode* node = m_pItemSystem->CreateParams();
		node->SetAttribute("name", "alienmount_acmo");
		node->SetAttribute("class", "Bullet");
		{
			IItemParamsNode* node_geometry = node->InsertChild("geometry");
			{
				IItemParamsNode* node_geometry_firstperson = node_geometry->InsertChild("firstperson");
				node_geometry_firstperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
			{
				IItemParamsNode* node_geometry_thirdperson = node_geometry->InsertChild("thirdperson");
				node_geometry_thirdperson->SetAttribute("name", "objects/weapons/alien/moac/bullet/bullet_long.cgf");
			}
		}
		{
			IItemParamsNode* node_flags = node->InsertChild("flags");
			{
				IItemParamsNode* node_flags_param = node_flags->InsertChild("param");
				node_flags_param->SetAttribute("name", "clientonly");
				node_flags_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_whiz = node->InsertChild("whiz");
			{
				IItemParamsNode* node_whiz_param = node_whiz->InsertChild("param");
				node_whiz_param->SetAttribute("name", "sound");
				node_whiz_param->SetAttribute("value", "sounds/physics:bullet_whiz:moac_whiz");
			}
		}
		{
			IItemParamsNode* node_physics = node->InsertChild("physics");
			node_physics->SetAttribute("type", "particle");
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "mass");
				node_physics_param->SetAttribute("value", static_cast<float>(1.0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "speed");
				node_physics_param->SetAttribute("value", static_cast<int>(180));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "air_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_resistance");
				node_physics_param->SetAttribute("value", static_cast<int>(0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "water_gravity");
				node_physics_param->SetAttribute("value", Vec3(0, 0, 0));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "pierceability");
				node_physics_param->SetAttribute("value", static_cast<int>(7));
			}
			{
				IItemParamsNode* node_physics_param = node_physics->InsertChild("param");
				node_physics_param->SetAttribute("name", "single_contact");
				node_physics_param->SetAttribute("value", static_cast<int>(1));
			}
		}
		{
			IItemParamsNode* node_params = node->InsertChild("params");
			{
				IItemParamsNode* node_params_param = node_params->InsertChild("param");
				node_params_param->SetAttribute("name", "lifetime");
				node_params_param->SetAttribute("value", static_cast<int>(15));
			}
		}
		{
			IItemParamsNode* node_collision = node->InsertChild("collision");
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "effect");
				node_collision_param->SetAttribute("value", "Alien_Weapons.Moac.Mount_Moac_Impact");
			}
			{
				IItemParamsNode* node_collision_param = node_collision->InsertChild("param");
				node_collision_param->SetAttribute("name", "scale");
				node_collision_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}
		{
			IItemParamsNode* node_trail = node->InsertChild("trail");
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "effect");
				node_trail_param->SetAttribute("value", "Alien_Weapons.Moac.Tracer_alienmount");
			}
			{
				IItemParamsNode* node_trail_param = node_trail->InsertChild("param");
				node_trail_param->SetAttribute("name", "scale");
				node_trail_param->SetAttribute("value", static_cast<float>(1.0));
			}
		}

		this->RegisterAmmo("alienmount_acmo", "Bullet", "", "", node);
	}
}
