#ifdef NEW_ITEM_SYSTEM
class ItemSystem;
using ItemSystemT = ItemSystem;
#else
class ItemSystemLegacy;
using ItemSystemT = ItemSystemLegacy;
#endif