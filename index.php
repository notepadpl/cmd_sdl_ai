<?php
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

$msg_file = __DIR__ . '/msg.txt';

function php_send_post_curl(string $url, string $payload, string $contentType = 'text/plain; charset=utf-8'): array {
    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_POST => true,
        CURLOPT_POSTFIELDS => $payload,
        CURLOPT_HTTPHEADER => [
            'Content-Type: ' . $contentType,
            'Content-Length: ' . strlen($payload),
        ],
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT => 10,
    ]);

    $body = curl_exec($ch);
    $err = curl_error($ch);
    $code = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    return [
        'ok' => $err === '' && $code >= 200 && $code < 300,
        'status' => $code,
        'error' => $err,
        'body' => $body,
    ];
}

if (isset($_GET['api']) && $_GET['api'] === '1') {
    header("Content-Type: text/plain; charset=utf-8");
    $method = $_SERVER['REQUEST_METHOD'];
    $action = $_GET['action'] ?? '';

    if ($action === 'clear' && ($method === 'POST' || $method === 'GET')) {
        if (@file_put_contents($msg_file, "", LOCK_EX) === false) {
            http_response_code(500);
            echo "ERR: cannot clear msg.txt";
            exit;
        }
        echo "OK: cleared";
        exit;
    }

    if ($method === 'POST') {
        $raw = file_get_contents('php://input');
        $msg = trim($raw !== '' ? $raw : ($_POST['msg'] ?? ''));

        if ($msg === '') {
            http_response_code(400);
            echo "ERR: empty message";
            exit;
        }

        if ($action === 'curl' && !empty($_GET['target_url'])) {
            $res = php_send_post_curl((string) $_GET['target_url'], $msg);
            if (!$res['ok']) {
                http_response_code(502);
                echo "ERR: curl failed ({$res['status']}) {$res['error']}";
                exit;
            }
            echo "OK CURL ({$res['status']}): " . ($res['body'] !== '' ? $res['body'] : $msg);
            exit;
        }

        if (@file_put_contents($msg_file, $msg . "\n", LOCK_EX) === false) {
            http_response_code(500);
            echo "ERR: cannot write msg.txt";
            exit;
        }

        echo "OK: " . $msg;
        exit;
    }

    if (file_exists($msg_file)) {
        readfile($msg_file);
    }
    exit;
}

header("Content-Type: text/html; charset=utf-8");
readfile(__DIR__ . '/index.html');