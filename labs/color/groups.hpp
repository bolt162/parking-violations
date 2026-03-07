#pragma once

#include<vector>

class Group {
private:
   int mID;
   std::vector<int> mMembers;

public:
   inline Group() : mID(-1) {}
   inline Group(int id) : mID(id) {}

   // cannot be copied/cloned
//   Group(const Group& m) = delete;
//   Group& operator=(const Group& m) = delete;

   inline void id(int id) {mID = id;}
   inline const int id() {return mID;}
   inline std::vector<int>& members() {return mMembers;}
};
