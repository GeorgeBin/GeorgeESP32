package com.george.esp32k.led.storage

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.emptyPreferences
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.longPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.core.stringSetPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.catch
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import java.io.IOException

private const val DEFAULT_DATASTORE_NAME = "infra_d_kv"
private val Context.appDataStore: DataStore<Preferences> by preferencesDataStore(name = DEFAULT_DATASTORE_NAME)

object KVManager {

    private lateinit var appContext: Context

    fun init(context: Context) {
        if (::appContext.isInitialized) return
        appContext = context.applicationContext
    }

    suspend fun putString(key: String, value: String?) {
        dataStore().edit { prefs ->
            if (value == null) {
                prefs.remove(stringPreferencesKey(key))
            } else {
                prefs[stringPreferencesKey(key)] = value
            }
        }
    }

    suspend fun getString(key: String, defaultValue: String? = null): String? {
        return dataStore().data
            .safeData()
            .map { it[stringPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeString(key: String, defaultValue: String? = null): Flow<String?> {
        return dataStore().data
            .safeData()
            .map { it[stringPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun putInt(key: String, value: Int) {
        dataStore().edit { it[intPreferencesKey(key)] = value }
    }

    suspend fun getInt(key: String, defaultValue: Int = 0): Int {
        return dataStore().data
            .safeData()
            .map { it[intPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeInt(key: String, defaultValue: Int = 0): Flow<Int> {
        return dataStore().data
            .safeData()
            .map { it[intPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun putLong(key: String, value: Long) {
        dataStore().edit { it[longPreferencesKey(key)] = value }
    }

    suspend fun getLong(key: String, defaultValue: Long = 0L): Long {
        return dataStore().data
            .safeData()
            .map { it[longPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeLong(key: String, defaultValue: Long = 0L): Flow<Long> {
        return dataStore().data
            .safeData()
            .map { it[longPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun putBoolean(key: String, value: Boolean) {
        dataStore().edit { it[booleanPreferencesKey(key)] = value }
    }

    suspend fun getBoolean(key: String, defaultValue: Boolean = false): Boolean {
        return dataStore().data
            .safeData()
            .map { it[booleanPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeBoolean(key: String, defaultValue: Boolean = false): Flow<Boolean> {
        return dataStore().data
            .safeData()
            .map { it[booleanPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun putFloat(key: String, value: Float) {
        dataStore().edit { it[floatPreferencesKey(key)] = value }
    }

    suspend fun getFloat(key: String, defaultValue: Float = 0f): Float {
        return dataStore().data
            .safeData()
            .map { it[floatPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeFloat(key: String, defaultValue: Float = 0f): Flow<Float> {
        return dataStore().data
            .safeData()
            .map { it[floatPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun putStringSet(key: String, value: Set<String>?) {
        dataStore().edit { prefs ->
            if (value == null) {
                prefs.remove(stringSetPreferencesKey(key))
            } else {
                prefs[stringSetPreferencesKey(key)] = value
            }
        }
    }

    suspend fun getStringSet(key: String, defaultValue: Set<String> = emptySet()): Set<String> {
        return dataStore().data
            .safeData()
            .map { it[stringSetPreferencesKey(key)] ?: defaultValue }
            .first()
    }

    fun observeStringSet(key: String, defaultValue: Set<String> = emptySet()): Flow<Set<String>> {
        return dataStore().data
            .safeData()
            .map { it[stringSetPreferencesKey(key)] ?: defaultValue }
    }

    suspend fun remove(key: String) {
        dataStore().edit { prefs ->
            prefs.remove(stringPreferencesKey(key))
            prefs.remove(intPreferencesKey(key))
            prefs.remove(longPreferencesKey(key))
            prefs.remove(booleanPreferencesKey(key))
            prefs.remove(floatPreferencesKey(key))
            prefs.remove(stringSetPreferencesKey(key))
        }
    }

    suspend fun contains(key: String): Boolean {
        return dataStore().data
            .safeData()
            .map { prefs ->
                listOf(
                    stringPreferencesKey(key),
                    intPreferencesKey(key),
                    longPreferencesKey(key),
                    booleanPreferencesKey(key),
                    floatPreferencesKey(key),
                    stringSetPreferencesKey(key)
                ).any { it in prefs }
            }
            .first()
    }

    suspend fun clear() {
        dataStore().edit { it.clear() }
    }

    private fun dataStore(): DataStore<Preferences> {
        check(::appContext.isInitialized) {
            "KVManager is not initialized. Call KVManager.init(context) in Application.onCreate()."
        }
        return appContext.appDataStore
    }
}

private fun Flow<Preferences>.safeData(): Flow<Preferences> {
    return catch { throwable ->
        if (throwable is IOException) {
            emit(emptyPreferences())
        } else {
            throw throwable
        }
    }
}
