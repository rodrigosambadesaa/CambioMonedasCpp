#if os(macOS)
import Cocoa

func normalize(_ s: String) -> String {
    let trimmed = s.trimmingCharacters(in: .whitespacesAndNewlines)
    let clean = trimmed.isEmpty ? "0" : trimmed
    let dropped = clean.drop(while: { $0 == "0" })
    return dropped.isEmpty ? "0" : String(dropped)
}

func isDigits(_ s: String) -> Bool {
    if s.isEmpty { return false }
    return s.allSatisfy { $0 >= "0" && $0 <= "9" }
}

func isIntegerToken(_ s: String) -> Bool {
    if s.isEmpty { return false }
    if s.first == "-" { return s.dropFirst().allSatisfy { $0 >= "0" && $0 <= "9" } }
    return isDigits(s)
}

func compareBig(_ a: String, _ b: String) -> Int {
    let x = normalize(a)
    let y = normalize(b)
    if x.count != y.count { return x.count < y.count ? -1 : 1 }
    if x == y { return 0 }
    return x < y ? -1 : 1
}

func addBig(_ a: String, _ b: String) -> String {
    let x = Array(normalize(a))
    let y = Array(normalize(b))
    var i = x.count - 1
    var j = y.count - 1
    var carry = 0
    var out: [Character] = []

    while i >= 0 || j >= 0 || carry > 0 {
        let da = i >= 0 ? Int(String(x[i]))! : 0
        let db = j >= 0 ? Int(String(y[j]))! : 0
        let s = da + db + carry
        out.append(Character(String(s % 10)))
        carry = s / 10
        i -= 1
        j -= 1
    }

    return String(out.reversed())
}

func subBig(_ a: String, _ b: String) -> String? {
    if compareBig(a, b) < 0 { return nil }

    let x = Array(normalize(a))
    let y = Array(normalize(b))
    var i = x.count - 1
    var j = y.count - 1
    var borrow = 0
    var out: [Character] = []

    while i >= 0 {
        var da = Int(String(x[i]))! - borrow
        let db = j >= 0 ? Int(String(y[j]))! : 0
        if da < db {
            da += 10
            borrow = 1
        } else {
            borrow = 0
        }

        out.append(Character(String(da - db)))
        i -= 1
        j -= 1
    }

    let res = String(out.reversed())
    return normalize(res)
}

func readTokens(from path: String) -> [String]? {
    guard let txt = try? String(contentsOfFile: path, encoding: .utf8) else { return nil }
    return txt.split { $0.isWhitespace }.map(String.init)
}

func writeTokens(_ tokens: [String], to path: String) -> Bool {
    let text = tokens.joined(separator: "\n") + "\n"
    do {
        try text.write(toFile: path, atomically: true, encoding: .utf8)
        return true
    } catch {
        return false
    }
}

func loadCoinNames() -> [String] {
    guard let tokens = readTokens(from: "monedas.txt") else { return [] }
    var names: [String] = []
    for t in tokens where !isIntegerToken(t) {
        if !names.contains(t) { names.append(t) }
    }
    return names
}

func loadSection(file: String, coin: String, minusOneAsZero: Bool) -> [String]? {
    guard let tokens = readTokens(from: file) else { return nil }
    guard let idx = tokens.firstIndex(of: coin) else { return nil }

    var out: [String] = []
    var i = idx + 1
    while i < tokens.count {
        var t = tokens[i]
        if !isIntegerToken(t) { break }
        if minusOneAsZero && t == "-1" { t = "0" }
        if !isDigits(t) { break }
        out.append(normalize(t))
        i += 1
    }

    return out.isEmpty ? nil : out
}

func updateStockSection(coin: String, stock: [String]) -> Bool {
    guard var tokens = readTokens(from: "stock.txt") else { return false }
    guard let idx = tokens.firstIndex(of: coin) else { return false }

    var i = idx + 1
    for s in stock {
        if i >= tokens.count || !isIntegerToken(tokens[i]) { return false }
        tokens[i] = normalize(s) == "0" ? "-1" : normalize(s)
        i += 1
    }

    return writeTokens(tokens, to: "stock.txt")
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSTableViewDataSource {
    var window: NSWindow!
    var coinPopup: NSPopUpButton!
    var denomPopup: NSPopUpButton!
    var qtyField: NSTextField!
    var table: NSTableView!
    var statusLabel: NSTextField!

    var coinNames: [String] = []
    var activeCoin: String?
    var denoms: [String] = []
    var stock: [String] = []

    func applicationDidFinishLaunching(_ notification: Notification) {
        setupUI()
        reloadCoins()
    }

    func setupUI() {
        window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 640, height: 460),
            styleMask: [.titled, .closable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.title = "ProgVoraz GUI macOS (Swift)"
        window.center()

        let content = window.contentView!

        let title = NSTextField(labelWithString: "Panel Administrador (macOS)")
        title.frame = NSRect(x: 20, y: 420, width: 300, height: 24)
        title.font = NSFont.boldSystemFont(ofSize: 16)
        content.addSubview(title)

        let coinLabel = NSTextField(labelWithString: "Moneda")
        coinLabel.frame = NSRect(x: 20, y: 388, width: 60, height: 22)
        content.addSubview(coinLabel)

        coinPopup = NSPopUpButton(frame: NSRect(x: 85, y: 385, width: 180, height: 26), pullsDown: false)
        content.addSubview(coinPopup)

        let loadBtn = NSButton(title: "Cargar", target: self, action: #selector(loadCoin))
        loadBtn.frame = NSRect(x: 275, y: 384, width: 90, height: 28)
        content.addSubview(loadBtn)

        let reloadBtn = NSButton(title: "Recargar", target: self, action: #selector(reloadCoinsAction))
        reloadBtn.frame = NSRect(x: 375, y: 384, width: 90, height: 28)
        content.addSubview(reloadBtn)

        let scroll = NSScrollView(frame: NSRect(x: 20, y: 150, width: 600, height: 220))
        table = NSTableView(frame: scroll.bounds)

        let col1 = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("denom"))
        col1.title = "Denominacion"
        col1.width = 260

        let col2 = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("stock"))
        col2.title = "Stock"
        col2.width = 320

        table.addTableColumn(col1)
        table.addTableColumn(col2)
        table.dataSource = self

        scroll.documentView = table
        scroll.hasVerticalScroller = true
        content.addSubview(scroll)

        let denomLabel = NSTextField(labelWithString: "Denominacion")
        denomLabel.frame = NSRect(x: 20, y: 110, width: 90, height: 22)
        content.addSubview(denomLabel)

        denomPopup = NSPopUpButton(frame: NSRect(x: 115, y: 107, width: 180, height: 26), pullsDown: false)
        content.addSubview(denomPopup)

        let qtyLabel = NSTextField(labelWithString: "Cantidad")
        qtyLabel.frame = NSRect(x: 310, y: 110, width: 70, height: 22)
        content.addSubview(qtyLabel)

        qtyField = NSTextField(frame: NSRect(x: 385, y: 106, width: 160, height: 26))
        content.addSubview(qtyField)

        let addBtn = NSButton(title: "Anadir", target: self, action: #selector(addStock))
        addBtn.frame = NSRect(x: 230, y: 62, width: 100, height: 30)
        content.addSubview(addBtn)

        let subBtn = NSButton(title: "Quitar", target: self, action: #selector(subStock))
        subBtn.frame = NSRect(x: 340, y: 62, width: 100, height: 30)
        content.addSubview(subBtn)

        statusLabel = NSTextField(labelWithString: "")
        statusLabel.frame = NSRect(x: 20, y: 20, width: 600, height: 24)
        content.addSubview(statusLabel)

        window.makeKeyAndOrderFront(nil)
    }

    func numberOfRows(in tableView: NSTableView) -> Int {
        return denoms.count
    }

    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        if row < 0 || row >= denoms.count { return nil }
        if tableColumn?.identifier.rawValue == "denom" {
            return denoms[row] + " c"
        }
        return stock[row]
    }

    func setStatus(_ text: String, error: Bool = false) {
        statusLabel.stringValue = text
        statusLabel.textColor = error ? .systemRed : .labelColor
    }

    func reloadCoins() {
        coinNames = loadCoinNames()
        coinPopup.removeAllItems()
        coinPopup.addItems(withTitles: coinNames)
        denoms = []
        stock = []
        denomPopup.removeAllItems()
        table.reloadData()

        if coinNames.isEmpty {
            setStatus("No se pudieron leer monedas desde monedas.txt", error: true)
        } else {
            setStatus("Monedas cargadas. Selecciona una y pulsa Cargar.")
        }
    }

    @objc func reloadCoinsAction() {
        reloadCoins()
    }

    @objc func loadCoin() {
        guard let coin = coinPopup.titleOfSelectedItem, !coin.isEmpty else {
            setStatus("Selecciona una moneda valida.", error: true)
            return
        }

        guard let d = loadSection(file: "monedas.txt", coin: coin, minusOneAsZero: false),
              let s = loadSection(file: "stock.txt", coin: coin, minusOneAsZero: true),
              d.count == s.count else {
            setStatus("No se pudo cargar denominaciones/stock.", error: true)
            return
        }

        activeCoin = coin
        denoms = d
        stock = s
        denomPopup.removeAllItems()
        denomPopup.addItems(withTitles: denoms)
        table.reloadData()
        setStatus("Moneda \(coin) cargada.")
    }

    func applyChange(isAdd: Bool) {
        guard let coin = activeCoin else {
            setStatus("Primero carga una moneda.", error: true)
            return
        }

        let idx = denomPopup.indexOfSelectedItem
        if idx < 0 || idx >= denoms.count {
            setStatus("Selecciona una denominacion valida.", error: true)
            return
        }

        let qty = normalize(qtyField.stringValue)
        if !isDigits(qty) {
            setStatus("Cantidad invalida: usa entero no negativo.", error: true)
            return
        }

        let old = stock[idx]
        if isAdd {
            stock[idx] = addBig(stock[idx], qty)
        } else {
            guard let v = subBig(stock[idx], qty) else {
                setStatus("No puedes quitar mas stock del disponible.", error: true)
                return
            }
            stock[idx] = v
        }

        if !updateStockSection(coin: coin, stock: stock) {
            stock[idx] = old
            setStatus("No se pudo persistir stock en stock.txt", error: true)
            return
        }

        qtyField.stringValue = ""
        table.reloadData()
        setStatus(isAdd ? "Stock actualizado (suma)." : "Stock actualizado (resta).")
    }

    @objc func addStock() {
        applyChange(isAdd: true)
    }

    @objc func subStock() {
        applyChange(isAdd: false)
    }
}

@main
struct ProgVorazMacOSApp {
    static func main() {
        let app = NSApplication.shared
        let delegate = AppDelegate()
        app.setActivationPolicy(.regular)
        app.delegate = delegate
        app.activate(ignoringOtherApps: true)
        app.run()
    }
}

#else
import Foundation
enum UnsupportedPlatformPlaceholder {}
#endif
