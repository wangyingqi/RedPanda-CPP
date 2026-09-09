-- Red Panda C++ compiler hint for macOS.
--
-- Discovers Homebrew GCC (preferred, matches Windows Dev-C++ behaviour:
-- <bits/stdc++.h>, libstdc++ diagnostics) and Xcode's Apple Clang.
-- Debugging uses lldb-dap from the Xcode Command Line Tools.
--
-- Installed into <app>/Contents/MacOS/compiler_hint.lua and executed by
-- Red Panda C++ when it searches for compilers (requires LUA_ADDON build).

function apiVersion()
   return {
      kind = "compiler_hint",
      major = 0,
      minor = 2,
   }
end

local profileNameMap = {
   release = { en_US = "release", zh_CN = "发布", zh_TW = "發佈" },
   debug   = { en_US = "debug",   zh_CN = "调试", zh_TW = "偵錯" },
}

local function profileName(lang, profile)
   local names = profileNameMap[profile]
   return names[lang] or names.en_US
end

local function findLldbDap()
   local candidates = {}
   local out, _, res = C_System.popen("/usr/bin/xcode-select", { "-p" }, { timeout = 3000 })
   if res and res.exitStatus == "NormalExit" and res.exitCode == 0 and out then
      local devDir = out:match("^%s*(.-)%s*$")
      if devDir ~= "" then
         table.insert(candidates, devDir .. "/usr/bin/lldb-dap")
         table.insert(candidates, devDir .. "/Toolchains/XcodeDefault.xctoolchain/usr/bin/lldb-dap")
      end
   end
   table.insert(candidates, "/Library/Developer/CommandLineTools/usr/bin/lldb-dap")
   table.insert(candidates, "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/lldb-dap")
   for _, path in ipairs(candidates) do
      if C_FileSystem.isExecutable(path) then
         return path
      end
   end
   return ""
end

local function dumpVersion(compiler)
   local out = C_System.popen(compiler, { "-dumpfullversion" }, { timeout = 5000 })
   if out then
      local v = out:match("^[0-9.]+")
      if v then return v end
   end
   out = C_System.popen(compiler, { "-dumpversion" }, { timeout = 5000 })
   if out then
      return out:match("^[0-9.]+") or ""
   end
   return ""
end

local function makeSets(lang, displayName, compilerType, cc, cxx, binDir, debugger, compatDir)
   local common = {
      cCompiler = cc,
      cxxCompiler = cxx,
      debugger = debugger,
      debugServer = "",
      make = "/usr/bin/make",
      compilerType = compilerType,
      preprocessingSuffix = ".i",
      compilationProperSuffix = ".s",
      assemblingSuffix = ".o",
      executableSuffix = "",
      compilationStage = 3,
      ccCmdOptUsePipe = "on",
      ccCmdOptNoMSExtensions = "on",
      ccCmdOptErrorReturnType = "on",
      ccCmdOptErrorImplicitInt = "on",
      ccCmdOptErrorVLA = "on",
      binDirs = { binDir, compatDir .. "/bin" },
      cIncludeDirs = { compatDir .. "/include" },
      cxxIncludeDirs = { compatDir .. "/include" },
   }
   local release = {
      name = displayName .. ", " .. profileName(lang, "release"),
      staticLink = false,
      linkCmdOptStripExe = "on",
      ccCmdOptOptimize = "2",
      customCompileParams = { "-DNDEBUG" },
   }
   local debug_ = {
      name = displayName .. ", " .. profileName(lang, "debug"),
      staticLink = false,
      ccCmdOptDebugInfo = "on",
      ccCmdOptWarningAll = "on",
      customCompileParams = { "-D_DEBUG" },
   }
   for k, v in pairs(common) do
      release[k] = v
      debug_[k] = v
   end
   return release, debug_
end

function main()
   local lang = C_Desktop.language()
   local compatDir = C_System.appResourceDir() .. "/compat"
   local debugger = findLldbDap()

   local compilerList = {}
   local preferCompiler = 0

   -- Homebrew GCC: /opt/homebrew/bin (Apple Silicon) or /usr/local/bin (Intel)
   local newestVersion = -1
   for _, binDir in ipairs({ "/opt/homebrew/bin", "/usr/local/bin" }) do
      local gxxs = C_FileSystem.matchFiles(binDir, "^g\\+\\+-[0-9]+$")
      for _, gxx in ipairs(gxxs) do
         local major = tonumber(gxx:sub(5))
         local gcc = binDir .. "/gcc-" .. gxx:sub(5)
         local cxx = binDir .. "/" .. gxx
         if major and C_FileSystem.isExecutable(gcc) and C_FileSystem.isExecutable(cxx) then
            local version = dumpVersion(cxx)
            local displayName = "GCC " .. version .. " (Homebrew)"
            local release, debug_ = makeSets(lang, displayName, "GCC_UTF8", gcc, cxx, binDir, debugger, compatDir)
            table.insert(compilerList, release)
            table.insert(compilerList, debug_)
            if major > newestVersion then
               newestVersion = major
               preferCompiler = #compilerList -- the debug profile of the newest GCC
            end
         end
      end
   end

   -- Apple Clang from Xcode Command Line Tools
   local clang = "/usr/bin/clang"
   if C_FileSystem.isExecutable(clang) then
      local version = dumpVersion(clang)
      local displayName = "Apple Clang " .. version
      local release, debug_ = makeSets(lang, displayName, "Clang", clang, "/usr/bin/clang++", "/usr/bin", debugger, compatDir)
      table.insert(compilerList, release)
      table.insert(compilerList, debug_)
      if preferCompiler == 0 then
         preferCompiler = #compilerList
      end
   end

   return {
      compilerList = compilerList,
      noSearch = { "/usr/bin", "/opt/homebrew/bin", "/usr/local/bin" },
      preferCompiler = preferCompiler,
   }
end
