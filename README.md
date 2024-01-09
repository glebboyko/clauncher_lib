# clauncher_lib

## Usage

### LNCR::LauncherServer
#### Constructor
1. Port *(int)*
2. Configuration file path (may not exist) *(const std::string&)*
3. Agent binary path *(const std::string&)*
4. *(optional)* logging_foo

### LauncherRunner

*Creates LauncherServer and enters into endless loop*

**Arguments:**
1. Port
2. Configuration file path (may not exist) *(const std::string&)*
3. Agent binary path *(const std::string&)*
4. *(optional)* logging_foo

### LNCR::LauncherClient
#### Constructor
1. Port *(int)*
2. *(optional)* logging_foo

#### LoadProcess
**Args**
1. Path to binary *(const std::string&)*
2. ProcessConfig
3. should wait for run *(bool)*

**Return value**
*(bool)*
- `true` process is launched successfully (or server accepted query while *should wait for run* is set to *false*)
- `false` process is not launched

#### StopProcess
**Args**
1. Path to binary *(const std::string&)*
2. should wait for stop *(bool)*

**Return value**

*(TermStatus)*
- `NoCheck`
- `SigTerm`
- `SigKill`
- `AlreadyTerminating`
- `NotRun`
- `NotRunning`
- `TermError`

#### ReRunProcess
**Args**
1. Path to binary *(const std::string&)*
2. should wait for rerun *(bool)*

**Return value** 
*(bool)*
- `true`
- `false`

#### IsProcessRunning
**Args**
1. Path to binary *(const std::string&)*

**Return value**
*(bool)*
- `true`
- `false`

#### GetProcessPid
**Args**
1. Path to binary *(const std::string&)*

**Return value**
*(std::optional\<int\>)*
- `! has value`
- `has value`

***
**logging_foo:**
`void(const std::string& module, const std::string& action, const std::string& event, int priority)`

**struct ProcessConfig**
- args *(std::list\<std::string\>)*
- should launch on boot *(bool)*
- should rerun on term *(bool)*
- time to stop *(std::optional\<std::chrono::milliseconds\>)*