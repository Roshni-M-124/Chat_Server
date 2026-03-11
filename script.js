let username = "";

const socket = new WebSocket("ws://localhost:8080");

socket.onopen = function() {
    console.log("Connected to server");
};

function startChat() {

    username = document.getElementById("nameInput").value;

    if (username === "") {
        alert("Please enter your name");
        return;
    }

    socket.send(username);
    window.location.href = "chat.html";
}