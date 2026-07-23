package com.finlink.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.URL
import java.util.Collections
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import org.json.JSONObject

private enum class SlotState { UNKNOWN, FREE, OCCUPIED, UNREACHABLE }

/**
 * Landing screen (three-page app: menu -> settings / player). Two ways to
 * find a host, both funnel into the same P1-P4 picker:
 *
 * 1. Manual host entry + "Suchen" (poll GET /status on all four player
 *    ports, docs/protocol.md).
 * 2. Discovery: scan the local subnet for a host answering on the lobby
 *    port (6800) -- Dolphin doesn't advertise itself (no mDNS/UPnP), so
 *    this is a plain sweep, not a real discovery protocol.
 *
 * Picking a free P slot starts PlayerActivity; the settings button opens
 * SettingsActivity. Neither owns any GbaStreamClient/native state -- that's
 * entirely PlayerActivity's job.
 *
 * No fixed orientation (see AndroidManifest.xml): this is a form, not the
 * stream view, so it should follow however the device is actually held.
 */
@OptIn(ExperimentalMaterial3Api::class)
class MenuActivity : ComponentActivity() {

    private var hostText by mutableStateOf("")
    private var searching by mutableStateOf(false)
    private var pickerVisible by mutableStateOf(false)
    private var slotStates by mutableStateOf(List(GbaStreamClient.PLAYER_SLOT_COUNT) { SlotState.UNKNOWN })
    private var statusText by mutableStateOf("")

    private var discovering by mutableStateOf(false)
    private var discoveryProgress by mutableStateOf(0f)
    private var discoveryStatusText by mutableStateOf("")
    private val discoveredServers = mutableStateListOf<String>()

    private var lastSearchedHost: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        statusText = getString(R.string.status_disconnected)

        setContent {
            FinlinkTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    Scaffold(
                        topBar = {
                            TopAppBar(
                                title = {
                                    Row(verticalAlignment = Alignment.CenterVertically) {
                                        Image(
                                            painter = painterResource(R.drawable.finlink_logo),
                                            contentDescription = null,
                                            modifier = Modifier.size(32.dp)
                                        )
                                        Spacer(Modifier.width(8.dp))
                                        Text(stringResource(R.string.app_name))
                                    }
                                },
                                actions = {
                                    TextButton(onClick = {
                                        startActivity(Intent(this@MenuActivity, SettingsActivity::class.java))
                                    }) {
                                        Text(stringResource(R.string.settings))
                                    }
                                }
                            )
                        }
                    ) { innerPadding ->
                        Column(modifier = Modifier.padding(innerPadding).padding(16.dp).fillMaxSize()) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                OutlinedTextField(
                                    value = hostText,
                                    onValueChange = { hostText = it },
                                    label = { Text(stringResource(R.string.host_hint)) },
                                    singleLine = true,
                                    modifier = Modifier.weight(1f)
                                )
                                Spacer(Modifier.width(8.dp))
                                Button(onClick = { searchLobby() }, enabled = !searching) {
                                    Text(stringResource(R.string.menu_connect))
                                }
                            }

                            if (pickerVisible) {
                                Spacer(Modifier.height(12.dp))
                                Row(
                                    modifier = Modifier.fillMaxWidth(),
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    slotStates.forEachIndexed { index, state ->
                                        Button(
                                            onClick = {
                                                lastSearchedHost?.let {
                                                    launchPlayer(it, GbaStreamClient.PLAYER_BASE_PORT + index)
                                                }
                                            },
                                            enabled = state == SlotState.FREE,
                                            modifier = Modifier.weight(1f)
                                        ) {
                                            Text("P${index + 1}")
                                        }
                                    }
                                }
                            }

                            Spacer(Modifier.height(8.dp))
                            Text(statusText, style = MaterialTheme.typography.bodyMedium)

                            Spacer(Modifier.height(16.dp))
                            HorizontalDivider()
                            Spacer(Modifier.height(16.dp))

                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(stringResource(R.string.discovery_title), modifier = Modifier.weight(1f))
                                Button(onClick = { startDiscovery() }, enabled = !discovering) {
                                    Text(stringResource(R.string.discovery_start))
                                }
                            }
                            Spacer(Modifier.height(4.dp))
                            if (discovering) {
                                LinearProgressIndicator(
                                    progress = { discoveryProgress },
                                    modifier = Modifier.fillMaxWidth()
                                )
                                Spacer(Modifier.height(4.dp))
                            }
                            Text(
                                discoveryStatusText,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )

                            Spacer(Modifier.height(8.dp))
                            LazyColumn(modifier = Modifier.weight(1f)) {
                                items(discoveredServers) { ip ->
                                    TextButton(
                                        onClick = { hostText = ip; runSearch(ip) },
                                        modifier = Modifier.fillMaxWidth()
                                    ) {
                                        Text(ip, modifier = Modifier.fillMaxWidth())
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun launchPlayer(host: String, port: Int) {
        val intent = Intent(this, PlayerActivity::class.java)
        intent.putExtra(PlayerActivity.EXTRA_HOST, host)
        intent.putExtra(PlayerActivity.EXTRA_PORT, port)
        startActivity(intent)
    }

    // --- Manual entry + P1-P4 picker (plain HTTP, not through finlink_core:
    // GET /status isn't part of the stream protocol). ---

    private fun searchLobby() {
        val host = hostText.trim()
        if (host.isEmpty()) {
            statusText = getString(R.string.status_error, getString(R.string.lobby_host_required))
            return
        }
        runSearch(host)
    }

    private fun runSearch(host: String) {
        hostText = host
        searching = true
        pickerVisible = false
        statusText = getString(R.string.lobby_searching)

        Thread {
            val occupied = (0 until GbaStreamClient.PLAYER_SLOT_COUNT).map { slot ->
                fetchOccupied(host, GbaStreamClient.PLAYER_BASE_PORT + slot)
            }
            runOnUiThread { applyLobbyResults(host, occupied) }
        }.start()
    }

    /** null = unreachable (port not configured as GBA (Client-Stream) at all, or unrelated error). */
    private fun fetchOccupied(host: String, port: Int): Boolean? {
        var connection: HttpURLConnection? = null
        return try {
            connection = (URL("http://$host:$port/status").openConnection() as HttpURLConnection).apply {
                connectTimeout = 1500
                readTimeout = 1500
                requestMethod = "GET"
            }
            val body = connection.inputStream.bufferedReader().use { it.readText() }
            JSONObject(body).optBoolean("occupied", false)
        } catch (e: Exception) {
            null
        } finally {
            connection?.disconnect()
        }
    }

    private fun applyLobbyResults(host: String, occupied: List<Boolean?>) {
        lastSearchedHost = host
        searching = false

        var anyFree = false
        slotStates = occupied.map { value ->
            when (value) {
                false -> { anyFree = true; SlotState.FREE }
                true -> SlotState.OCCUPIED
                null -> SlotState.UNREACHABLE
            }
        }
        pickerVisible = true
        statusText = if (anyFree) getString(R.string.lobby_pick) else getString(R.string.lobby_none_free)
    }

    // --- Discovery: sweep the local /24 (or smaller) subnet for a host
    // answering on the lobby port. This is not a real discovery protocol --
    // Dolphin's server doesn't broadcast/advertise itself in any way (no
    // mDNS/UPnP/SSDP), so a plain port sweep is the only option. ---

    private fun startDiscovery() {
        discovering = true
        discoveryProgress = 0f
        discoveredServers.clear()
        discoveryStatusText = getString(R.string.discovery_scanning)

        Thread {
            val hosts = localSubnetHosts()
            if (hosts.isEmpty()) {
                runOnUiThread {
                    discovering = false
                    discoveryStatusText = getString(R.string.discovery_no_subnet)
                }
                return@Thread
            }

            val executor = Executors.newFixedThreadPool(32)
            val foundCount = AtomicInteger(0)
            val doneCount = AtomicInteger(0)
            val latch = CountDownLatch(hosts.size)

            for (ip in hosts) {
                executor.submit {
                    try {
                        if (probeLobby(ip)) {
                            foundCount.incrementAndGet()
                            runOnUiThread { discoveredServers.add(ip) }
                        }
                    } finally {
                        val done = doneCount.incrementAndGet()
                        runOnUiThread {
                            discoveryProgress = done.toFloat() / hosts.size.toFloat()
                        }
                        latch.countDown()
                    }
                }
            }

            latch.await(30, TimeUnit.SECONDS)
            executor.shutdownNow()

            runOnUiThread {
                discovering = false
                discoveryStatusText = if (foundCount.get() == 0) {
                    getString(R.string.discovery_none_found)
                } else {
                    getString(R.string.discovery_found, foundCount.get())
                }
            }
        }.start()
    }

    private fun probeLobby(ip: String): Boolean {
        var connection: HttpURLConnection? = null
        return try {
            connection = (URL("http://$ip:6800/").openConnection() as HttpURLConnection).apply {
                connectTimeout = 400
                readTimeout = 400
                requestMethod = "GET"
            }
            connection.responseCode == 200
        } catch (e: Exception) {
            false
        } finally {
            connection?.disconnect()
        }
    }

    /** Host addresses (excluding network/broadcast) on this device's local
     * IPv4 subnet, or empty if none found / the subnet is implausibly large
     * (a misdetected /8 etc. would otherwise mean scanning millions of hosts). */
    private fun localSubnetHosts(): List<String> {
        val interfaces = Collections.list(NetworkInterface.getNetworkInterfaces() ?: return emptyList())
        for (iface in interfaces) {
            if (iface.isLoopback || !iface.isUp) continue
            for (addr in iface.interfaceAddresses) {
                val ip = addr.address
                if (ip is Inet4Address && addr.networkPrefixLength in 22..30) {
                    return hostsInSubnet(ip.hostAddress ?: continue, addr.networkPrefixLength.toInt())
                }
            }
        }
        return emptyList()
    }

    private fun hostsInSubnet(ip: String, prefixLength: Int): List<String> {
        val parts = ip.split(".").map { it.toIntOrNull() ?: return emptyList() }
        if (parts.size != 4) return emptyList()
        val ipInt = (parts[0] shl 24) or (parts[1] shl 16) or (parts[2] shl 8) or parts[3]
        val hostBits = 32 - prefixLength
        val maxHosts = (1 shl hostBits) - 2 // exclude network + broadcast address
        if (maxHosts <= 0) return emptyList()
        val networkInt = ipInt and (-1 shl hostBits)
        return (1..maxHosts).map { offset ->
            val hostInt = networkInt + offset
            "${(hostInt shr 24) and 0xFF}.${(hostInt shr 16) and 0xFF}.${(hostInt shr 8) and 0xFF}.${hostInt and 0xFF}"
        }
    }
}
