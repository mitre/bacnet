# Caldera for OT plugin: BACnet

A [Caldera for OT](https://github.com/mitre/caldera-ot) plugin supplying [Caldera](https://github.com/mitre/caldera) with BACnet protocol TTPs.
This is part of a series of plugins that provide added threat emulation capability for Operational Technology (OT) environments.

Full BACnet plugin [documentation](docs/bacnet.md) can be viewed as part of fieldmanual, once the Caldera server is running.

## Installation

To run Caldera along with BACnet plugin:
1. Download Caldera as detailed in the [Installation Guide](https://github.com/mitre/caldera)
2. Install the bacnet plugin in Caldera's plugin directory: `caldera/plugins`
3. Enable the bacnet plugin by adding `- bacnet` to the list of enabled plugins in `conf/local.yml` or `conf/default.yml` (if running Caldera in insecure mode)

### Version
This plugin is compatible with Caldera v4.2.0 and v5.0.0. The latest version of Caldera can be checked out using the following method:
```
git clone --recursive https://github.com/mitre/caldera.git
```
### Tested OS Versions for Plugin Payload(s)

Building of the BACnet plugin payloads has been tested as described [here](/src/README.md#reproducing-builds). See the corresponding plugin payload source code for further build information.

Testing of the binaries has occured on:
* Microsoft Windows 10 v21H2
* Ubuntu 22.04.2 LTS

## Plugin Usage
 - Import the plugin, and optionally set up the required facts (i.e. like the fact sources provided).
 - Start an operation, optionally using the fact source you set up.
 - Use "Add Potential Link" to run a specific ability from this plugin. You can enter the fact values manually, or use the ones from your fact source.

 Sources contains a small [example fact set](/data/sources/ddc9cb50-74b7-4f32-9ed1-39bb0a58c954.yml) and the fieldmanual documentation contains a reference section on [BACnet sources.](/docs/bacnet.md#bacnet-sources-and-facts)